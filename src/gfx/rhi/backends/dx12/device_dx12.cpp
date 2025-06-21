#include "gfx/rhi/backends/dx12/device_dx12.h"

#include "platform/windows/windows_platform_setup.h"

#ifdef ARISE_RHI_DX12

#include "gfx/rhi/backends/dx12/buffer_dx12.h"
#include "gfx/rhi/backends/dx12/command_buffer_dx12.h"
#include "gfx/rhi/backends/dx12/descriptor_dx12.h"
#include "gfx/rhi/backends/dx12/framebuffer_dx12.h"
#include "gfx/rhi/backends/dx12/pipeline_dx12.h"
#include "gfx/rhi/backends/dx12/render_pass_dx12.h"
#include "gfx/rhi/backends/dx12/rhi_enums_dx12.h"
#include "gfx/rhi/backends/dx12/sampler_dx12.h"
#include "gfx/rhi/backends/dx12/shader_dx12.h"
#include "gfx/rhi/backends/dx12/swap_chain_dx12.h"
#include "gfx/rhi/backends/dx12/synchronization_dx12.h"
#include "gfx/rhi/backends/dx12/texture_dx12.h"
#include "platform/common/window.h"
//#include "profiler/backends/gpu_profiler_dx12.h"
#include "profiler/backends/gpu_profiler.h"
#include "utils/logger/global_logger.h"
#include "utils/service/service_locator.h"

namespace arise {
namespace gfx {
namespace rhi {

constexpr uint32_t DESCRIPTOR_HEAP_CAPACITY_CPU_RTV         = 256;
constexpr uint32_t DESCRIPTOR_HEAP_CAPACITY_CPU_DSV         = 256;
constexpr uint32_t DESCRIPTOR_HEAP_CAPACITY_CPU_CBV_SRV_UAV = 10'000;
constexpr uint32_t DESCRIPTOR_HEAP_CAPACITY_CPU_SAMPLER     = 256;

constexpr uint32_t COMMAND_ALLOCATOR_POOL_SIZE = 8;

DeviceDx12::DeviceDx12(const DeviceDesc& desc)
    : Device(desc) {
#ifdef _DEBUG
  {
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
      debugController->EnableDebugLayer();

      ComPtr<ID3D12Debug1> debugController1;
      if (SUCCEEDED(debugController.As(&debugController1))) {
        debugController1->SetEnableGPUBasedValidation(TRUE);
      }
    }
  }
#endif

  if (!createFactory_() || !createDevice_() || !createAllocator_() || !createCommandQueue_() || !createCommandPools_()
      || !createDescriptorHeaps_()) {
    GlobalLogger::Log(LogLevel::Error, "Failed to initialize DirectX 12 device");
  }
}

DeviceDx12::~DeviceDx12() {
  waitIdle();

  m_cpuRtvHeap.release();
  m_cpuDsvHeap.release();
  m_cpuCbvSrvUavHeap.release();
  m_cpuSamplerHeap.release();

  m_frameResourcesManager.release();

  m_commandQueue_.Reset();
  m_device_.Reset();
  m_adapter_.Reset();
  m_factory_.Reset();
}

bool DeviceDx12::createFactory_() {
  UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
  dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory_));
  if (FAILED(hr)) {
    // TODO: add logger
    return false;
  }

  return true;
}

bool DeviceDx12::findAdapter_(IDXGIAdapter1** adapter) {
  for (UINT i = 0;; ++i) {
    ComPtr<IDXGIAdapter1> currentAdapter;

    if (m_factory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&currentAdapter))
        == DXGI_ERROR_NOT_FOUND) {
      break;
    }

    if (SUCCEEDED(D3D12CreateDevice(currentAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
      DXGI_ADAPTER_DESC1 desc;
      currentAdapter->GetDesc1(&desc);

      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue;
      }

#ifdef _DEBUG
      wchar_t debugMessage[256];
      swprintf_s(debugMessage, L"Selected DirectX 12 Adapter: %s\n", desc.Description);
      OutputDebugStringW(debugMessage);
#endif

      *adapter = currentAdapter.Detach();
      return true;
    }
  }

  for (UINT i = 0;; ++i) {
    ComPtr<IDXGIAdapter1> currentAdapter;

    if (m_factory_->EnumAdapters1(i, &currentAdapter) == DXGI_ERROR_NOT_FOUND) {
      break;
    }

    DXGI_ADAPTER_DESC1 desc;
    currentAdapter->GetDesc1(&desc);

    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
      continue;
    }

    if (SUCCEEDED(D3D12CreateDevice(currentAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
#ifdef _DEBUG
      wchar_t debugMessage[256];
      swprintf_s(debugMessage, L"Selected DirectX 12 Adapter (fallback): %s\n", desc.Description);
      OutputDebugStringW(debugMessage);
#endif

      *adapter = currentAdapter.Detach();
      return true;
    }
  }

  return false;
}

bool DeviceDx12::createDevice_() {
  ComPtr<IDXGIAdapter1> adapter;
  if (!findAdapter_(adapter.GetAddressOf())) {
    GlobalLogger::Log(LogLevel::Error, "No suitable DirectX 12 adapter found!");
    return false;
  }

  adapter.As(&m_adapter_);

  HRESULT hr = D3D12CreateDevice(m_adapter_.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device_));

  if (FAILED(hr)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create DirectX 12 device!");
    return false;
  }

#ifdef _DEBUG
  ComPtr<ID3D12InfoQueue> infoQueue;
  if (SUCCEEDED(m_device_.As(&infoQueue))) {
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

    D3D12_MESSAGE_ID suppressIds[] = {D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE, D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE};

    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumIDs         = std::size(suppressIds);
    filter.DenyList.pIDList        = suppressIds;
    infoQueue->AddStorageFilterEntries(&filter);
  }
#endif

  return true;
}

bool DeviceDx12::createCommandQueue_() {
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queueDesc.NodeMask                 = 0;

  HRESULT hr = m_device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue_));
  if (FAILED(hr)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create DirectX 12 command queue");
    return false;
  }

  return true;
}

bool DeviceDx12::createCommandPools_() {
  return m_commandAllocatorManager_.initialize(m_device_.Get(), COMMAND_ALLOCATOR_POOL_SIZE);
}

bool DeviceDx12::createDescriptorHeaps_() {
  if (!m_cpuRtvHeap.initialize(
          m_device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, DESCRIPTOR_HEAP_CAPACITY_CPU_RTV, false)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create RTV descriptor heap");
    return false;
  }

  if (!m_cpuDsvHeap.initialize(
          m_device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, DESCRIPTOR_HEAP_CAPACITY_CPU_DSV, false)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create DSV descriptor heap");
    return false;
  }

  if (!m_cpuCbvSrvUavHeap.initialize(
          m_device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, DESCRIPTOR_HEAP_CAPACITY_CPU_CBV_SRV_UAV, false)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create CPU CBV/SRV/UAV descriptor heap");
    return false;
  }

  if (!m_cpuSamplerHeap.initialize(
          m_device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, DESCRIPTOR_HEAP_CAPACITY_CPU_SAMPLER, false)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create CPU Sampler descriptor heap");
    return false;
  }

  return true;
}

bool DeviceDx12::createAllocator_() {
  D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
  allocatorDesc.pDevice                 = m_device_.Get();
  allocatorDesc.pAdapter                = m_adapter_.Get();

  HRESULT hr = D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator_);
  if (FAILED(hr)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create D3D12 Memory Allocator");
    return false;
  }

  return true;
}

std::unique_ptr<Buffer> DeviceDx12::createBuffer(const BufferDesc& desc) {
  bool isDirectBuffer
      = (desc.createFlags
         & (BufferCreateFlag::VertexBuffer | BufferCreateFlag::IndexBuffer | BufferCreateFlag::InstanceBuffer))
     != BufferCreateFlag::None;

  bool isDescriptorBuffer
      = (desc.createFlags
         & (BufferCreateFlag::ConstantBuffer | BufferCreateFlag::Uav | BufferCreateFlag::ShaderResource))
     != BufferCreateFlag::None;

  if (isDescriptorBuffer) {
    return std::make_unique<DescriptorBufferDx12>(desc, this);
  } else if (isDirectBuffer) {
    return std::make_unique<DirectBufferDx12>(desc, this);
  }

  return std::make_unique<BufferDx12>(desc, this);
}

std::unique_ptr<Texture> DeviceDx12::createTexture(const TextureDesc& desc) {
  return std::make_unique<TextureDx12>(desc, this);
}

std::unique_ptr<Sampler> DeviceDx12::createSampler(const SamplerDesc& desc) {
  return std::make_unique<SamplerDx12>(desc, this);
}

std::unique_ptr<Shader> DeviceDx12::createShader(const ShaderDesc& desc) {
  return std::make_unique<ShaderDx12>(desc, this);
}

std::unique_ptr<GraphicsPipeline> DeviceDx12::createGraphicsPipeline(const GraphicsPipelineDesc& desc) {
  return std::make_unique<GraphicsPipelineDx12>(desc, this);
}

std::unique_ptr<DescriptorSetLayout> DeviceDx12::createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) {
  return std::make_unique<DescriptorSetLayoutDx12>(desc, this);
}

std::unique_ptr<DescriptorSet> DeviceDx12::createDescriptorSet(const DescriptorSetLayout* layout) {
  const DescriptorSetLayoutDx12* descriptorSetLayoutDx12 = dynamic_cast<const DescriptorSetLayoutDx12*>(layout);
  if (!descriptorSetLayoutDx12) {
    return nullptr;
  }

  return std::make_unique<DescriptorSetDx12>(this, descriptorSetLayoutDx12);
}

std::unique_ptr<RenderPass> DeviceDx12::createRenderPass(const RenderPassDesc& desc) {
  return std::make_unique<RenderPassDx12>(desc, this);
}

std::unique_ptr<Framebuffer> DeviceDx12::createFramebuffer(const FramebufferDesc& desc) {
  return std::make_unique<FramebufferDx12>(desc, this);
}

std::unique_ptr<CommandBuffer> DeviceDx12::createCommandBuffer(const CommandBufferDesc& desc) {
  // NOTE: This implementation is not thread-safe.
  // In a production multi-threaded engine, you would need to protect
  // the command allocator pool with a mutex.

  ID3D12CommandAllocator* commandAllocator = m_commandAllocatorManager_.getCommandAllocator();
  if (!commandAllocator) {
    return nullptr;
  }

  ComPtr<ID3D12GraphicsCommandList> commandList;
  HRESULT                           hr
      = m_device_->CreateCommandList(0,
                                     desc.primary ? D3D12_COMMAND_LIST_TYPE_DIRECT : D3D12_COMMAND_LIST_TYPE_BUNDLE,
                                     commandAllocator,
                                     nullptr,
                                     IID_PPV_ARGS(&commandList));

  if (FAILED(hr)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create command list");
    m_commandAllocatorManager_.returnCommandAllocator(commandAllocator);
    return nullptr;
  }

  commandList->Close();

  return std::make_unique<CommandBufferDx12>(this, std::move(commandList), commandAllocator);
}

std::unique_ptr<Fence> DeviceDx12::createFence(const FenceDesc& desc) {
  return std::make_unique<FenceDx12>(desc, this);
}

std::unique_ptr<Semaphore> DeviceDx12::createSemaphore() {
  // In DX12 implementation, semaphores are implemented as fences
  return std::make_unique<SemaphoreDx12>(this);
}

std::unique_ptr<SwapChain> DeviceDx12::createSwapChain(const SwapchainDesc& desc) {
  m_frameResourcesManager.release();
  if (!m_frameResourcesManager.initialize(this, desc.bufferCount)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to initialize frame resources manager for DX12");
    return nullptr;
  }
  return std::make_unique<SwapChainDx12>(desc, this);
}

void DeviceDx12::updateBuffer(Buffer* buffer, const void* data, size_t size, size_t offset) {
  BufferDx12* bufferDx12 = dynamic_cast<BufferDx12*>(buffer);
  if (!bufferDx12) {
    GlobalLogger::Log(LogLevel::Error, "Invalid buffer type");
    return;
  }

  if (!data || size == 0) {
    GlobalLogger::Log(LogLevel::Warning, "No data to update");
    return;
  }

  if (offset + size > bufferDx12->getSize()) {
    GlobalLogger::Log(LogLevel::Error, "Update exceeds buffer size");
    return;
  }

  if (bufferDx12->isUploadHeapBuffer_()) {
    if (bufferDx12->update_(data, size, offset)) {
      return;
    }
  }

  // For default heap buffers, use a staging (temporary upload) buffer and copy

  BufferDesc uploadDesc;
  uploadDesc.size        = size;
  uploadDesc.type        = BufferType::Dynamic;
  uploadDesc.createFlags = BufferCreateFlag::CpuAccess;
  uploadDesc.debugName   = "staging_buffer";

  std::unique_ptr<Buffer> stagingBuffer     = createBuffer(uploadDesc);
  BufferDx12*             stagingBufferDx12 = static_cast<BufferDx12*>(stagingBuffer.get());

  if (!stagingBufferDx12->update_(data, size, 0)) {
    GlobalLogger::Log(LogLevel::Error, "updateBuffer: Failed to update staging buffer");
    return;
  }

  CommandBufferDesc cmdBufferDesc;
  cmdBufferDesc.primary = true;
  auto cmdBuffer        = createCommandBuffer(cmdBufferDesc);

  cmdBuffer->reset();
  cmdBuffer->begin();
  cmdBuffer->copyBuffer(stagingBuffer.get(), buffer, 0, offset, size);
  cmdBuffer->end();

  FenceDesc fenceDesc;
  auto      fence = createFence(fenceDesc);
  submitCommandBuffer(cmdBuffer.get(), fence.get());
  fence->wait();
}

void DeviceDx12::updateTexture(
    Texture* texture, const void* data, size_t dataSize, uint32_t mipLevel, uint32_t arrayLayer) {
  TextureDx12* textureDx12 = dynamic_cast<TextureDx12*>(texture);
  if (!textureDx12) {
    return;
  }

  textureDx12->update(data, dataSize, mipLevel, arrayLayer);
}

void DeviceDx12::submitCommandBuffer(CommandBuffer*                 cmdBuffer,
                                     Fence*                         signalFence,
                                     const std::vector<Semaphore*>& waitSemaphores,
                                     const std::vector<Semaphore*>& signalSemaphores) {
  CommandBufferDx12* cmdBufferDx12 = dynamic_cast<CommandBufferDx12*>(cmdBuffer);
  if (!cmdBufferDx12) {
    return;
  }

  ID3D12CommandList* ppCommandLists[] = {cmdBufferDx12->getCommandList()};

  {
    std::lock_guard<std::mutex> lock(m_queueSubmitMutex);
    m_commandQueue_->ExecuteCommandLists(std::size(ppCommandLists), ppCommandLists);
  }

  if (signalFence) {
    FenceDx12* fenceDx12 = dynamic_cast<FenceDx12*>(signalFence);
    if (fenceDx12) {
      fenceDx12->signal(m_commandQueue_.Get());
    }
  }

  for (Semaphore* semaphore : signalSemaphores) {
    SemaphoreDx12* semaphoreDx12 = dynamic_cast<SemaphoreDx12*>(semaphore);
    if (semaphoreDx12) {
      semaphoreDx12->signal(m_commandQueue_.Get());
    }
  }
}

void DeviceDx12::waitIdle() {
  ComPtr<ID3D12Fence> fence;
  HRESULT             hr = m_device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  if (FAILED(hr)) {
    return;
  }

  HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!eventHandle) {
    return;
  }

  const UINT64 fenceValue = 1;
  m_commandQueue_->Signal(fence.Get(), fenceValue);

  if (fence->GetCompletedValue() < fenceValue) {
    fence->SetEventOnCompletion(fenceValue, eventHandle);
    WaitForSingleObject(eventHandle, INFINITE);
  }

  CloseHandle(eventHandle);
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_RHI_DX12