#include "gfx/rhi/backends/dx12/descriptor_dx12.h"

#ifdef ARISE_USE_DX12

#include "gfx/rhi/backends/dx12/buffer_dx12.h"
#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "gfx/rhi/backends/dx12/rhi_enums_dx12.h"
#include "gfx/rhi/backends/dx12/sampler_dx12.h"
#include "gfx/rhi/backends/dx12/texture_dx12.h"
#include "utils/logger/global_logger.h"

#include <algorithm>

namespace arise {
namespace gfx {
namespace rhi {

//-------------------------------------------------------------------------
// DescriptorSetLayoutDx12 implementation
//-------------------------------------------------------------------------

DescriptorSetLayoutDx12::DescriptorSetLayoutDx12(const DescriptorSetLayoutDesc& desc, DeviceDx12* device)
    : DescriptorSetLayout(desc)
    , m_device_(device) {
  if (desc.bindings.empty()) {
    GlobalLogger::Log(LogLevel::Error, "No bindings provided");
    return;
  }

  D3D12_DESCRIPTOR_RANGE_TYPE firstType = g_getShaderBindingTypeDx12(desc.bindings[0].type);
  m_isSamplerLayout                     = (firstType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER);

  for (const auto& binding : desc.bindings) {
    D3D12_DESCRIPTOR_RANGE_TYPE type             = g_getShaderBindingTypeDx12(binding.type);
    bool                        bindingIsSampler = (type == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER);

    if (bindingIsSampler != m_isSamplerLayout) {
      GlobalLogger::Log(LogLevel::Error,
                        "Mixed CBV/SRV/UAV and Sampler bindings not allowed in same layout (invariant). "
                        "Descriptor layout is marked as "
                            + std::string(m_isSamplerLayout ? "sampler" : "non-sampler") + " but binding "
                            + std::to_string(binding.binding) + " is "
                            + std::string(bindingIsSampler ? "sampler" : "non-sampler"));
      return;
    }
  }

  for (const auto& binding : desc.bindings) {
    D3D12_DESCRIPTOR_RANGE_TYPE type = g_getShaderBindingTypeDx12(binding.type);
    m_bindingsByType[type].push_back(binding);
  }

  for (auto& [type, bindings] : m_bindingsByType) {
    std::sort(bindings.begin(),
              bindings.end(),
              [](const DescriptorSetLayoutBindingDesc& a, const DescriptorSetLayoutBindingDesc& b) {
                return a.binding < b.binding;
              });

    for (const auto& b : bindings) {
      m_totalDescriptors_ += b.descriptorCount;
    }
  }

  std::vector<D3D12_DESCRIPTOR_RANGE_TYPE> rangeOrder;
  if (m_isSamplerLayout) {
    rangeOrder = {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER};
  } else {
    rangeOrder = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_DESCRIPTOR_RANGE_TYPE_CBV};
  }

  for (auto type : rangeOrder) {
    auto it = m_bindingsByType.find(type);
    if (it == m_bindingsByType.end() || it->second.empty()) {
      continue;
    }

    const auto& bindings = it->second;

    uint32_t minBinding = UINT32_MAX;
    uint32_t maxBinding = 0;

    for (const auto& binding : bindings) {
      minBinding = std::min(minBinding, binding.binding);
      maxBinding = std::max(maxBinding, binding.binding + binding.descriptorCount - 1);
    }

    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType                         = type;
    range.NumDescriptors                    = maxBinding - minBinding + 1;
    range.BaseShaderRegister                = minBinding;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    m_descriptorRanges.push_back(range);
  }
}

//-------------------------------------------------------------------------
// DescriptorSetDx12 implementation
//-------------------------------------------------------------------------

DescriptorSetDx12::DescriptorSetDx12(DeviceDx12* device, const DescriptorSetLayoutDx12* layout)
    : m_device_(device)
    , m_layout_(layout) {
  uint32_t frameCount = m_device_->getFrameResourcesManager()->getFrameCount();

  if (layout->isSamplerLayout()) {
    m_samplerIndices_.assign(frameCount, UINT32_MAX);
  } else {
    m_srvUavCbvIndices_.assign(frameCount, UINT32_MAX);
  }
}

DescriptorSetDx12::~DescriptorSetDx12() {
  auto* frameResMgr = m_device_->getFrameResourcesManager();

  for (std::size_t i = 0; i < m_srvUavCbvIndices_.size(); ++i) {
    if (m_srvUavCbvIndices_[i] != UINT32_MAX) {
      frameResMgr->getCbvSrvUavHeap(i)->freeBlock(m_srvUavCbvIndices_[i], m_layout_->getTotalDescriptors());
    }
  }

  for (std::size_t i = 0; i < m_samplerIndices_.size(); ++i) {
    if (m_samplerIndices_[i] != UINT32_MAX) {
      frameResMgr->getSamplerHeap(i)->freeBlock(m_samplerIndices_[i], m_layout_->getTotalDescriptors());
    }
  }
}

void DescriptorSetDx12::setUniformBuffer(uint32_t binding, Buffer* buffer, uint64_t offset, uint64_t range) {
  if (!buffer) {
    GlobalLogger::Log(LogLevel::Error, "Null buffer");
    return;
  }

  DescriptorBufferDx12* bufferDx12 = dynamic_cast<DescriptorBufferDx12*>(buffer);
  if (!bufferDx12) {
    GlobalLogger::Log(LogLevel::Error, "Invalid buffer type");
    return;
  }

  if (!bufferDx12->hasCbvHandle()) {
    GlobalLogger::Log(LogLevel::Error, "Buffer does not have a CBV");
    return;
  }

  auto*    frameResMgr  = m_device_->getFrameResourcesManager();
  uint32_t currentFrame = frameResMgr->getCurrentFrameIndex();
  if (m_srvUavCbvIndices_[currentFrame] == UINT32_MAX) {
    uint32_t totalDesc                = m_layout_->getTotalDescriptors();
    m_srvUavCbvIndices_[currentFrame] = frameResMgr->getCbvSrvUavHeap(currentFrame)->allocate(totalDesc);
  }

  DescriptorHeapDx12* cpuHeap = m_device_->getCpuCbvSrvUavHeap();
  DescriptorHeapDx12* gpuHeap = m_device_->getFrameResourcesManager()->getCurrentCbvSrvUavHeap();

  if (!cpuHeap || !gpuHeap) {
    GlobalLogger::Log(LogLevel::Error, "Descriptor heaps not available");
    return;
  }

  uint32_t srcIndex = (bufferDx12->getCbvCpuHandle().ptr - cpuHeap->getCpuHandle(0).ptr) / cpuHeap->getDescriptorSize();

  uint32_t bindingOffset = findBindingOffset_(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, binding);
  uint32_t dstIndex      = m_srvUavCbvIndices_[currentFrame] + bindingOffset;

  gpuHeap->copyDescriptors(cpuHeap, srcIndex, dstIndex, 1);
}

void DescriptorSetDx12::setTextureSampler(uint32_t binding, Texture* texture, Sampler* sampler) {
  // For DX12, we need to handle this by setting texture and sampler separately
  // This is because DX12 handles samplers and SRVs through different tables
  // and we have an invariant that 1 descriptor set is 1 descriptor table (hence, either sampler or cbv/srv/uav)
  GlobalLogger::Log(LogLevel::Info, "Setting texture and sampler separately for DX12");
}

void DescriptorSetDx12::setTexture(uint32_t binding, Texture* texture, ResourceLayout layout) {
  if (!texture) {
    GlobalLogger::Log(LogLevel::Error, "Null texture");
    return;
  }

  if (m_layout_->isSamplerLayout()) {
    GlobalLogger::Log(LogLevel::Error, "Cannot set texture on a sampler descriptor set");
    return;
  }

  TextureDx12* textureDx12 = dynamic_cast<TextureDx12*>(texture);
  if (!textureDx12) {
    GlobalLogger::Log(LogLevel::Error, "Invalid texture type");
    return;
  }

  auto*    frameResMgr  = m_device_->getFrameResourcesManager();
  uint32_t currentFrame = frameResMgr->getCurrentFrameIndex();
  if (m_srvUavCbvIndices_[currentFrame] == UINT32_MAX) {
    uint32_t totalDesc                = m_layout_->getTotalDescriptors();
    m_srvUavCbvIndices_[currentFrame] = frameResMgr->getCbvSrvUavHeap(currentFrame)->allocate(totalDesc);
  }

  DescriptorHeapDx12* cpuHeap = m_device_->getCpuCbvSrvUavHeap();
  DescriptorHeapDx12* gpuHeap = frameResMgr->getCurrentCbvSrvUavHeap();
  if (!cpuHeap || !gpuHeap) {
    GlobalLogger::Log(LogLevel::Error, "Descriptor heaps not available");
    return;
  }

  uint32_t srcIndex = textureDx12->getSrvDescriptorIndex();

  uint32_t bindingOffset = findBindingOffset_(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, binding);
  uint32_t dstIndex      = m_srvUavCbvIndices_[currentFrame] + bindingOffset;

  gpuHeap->copyDescriptors(cpuHeap, srcIndex, dstIndex, 1);
}

void DescriptorSetDx12::setSampler(uint32_t binding, Sampler* sampler) {
  if (!sampler) {
    GlobalLogger::Log(LogLevel::Error, "Null sampler");
    return;
  }
  if (!m_layout_->isSamplerLayout()) {
    GlobalLogger::Log(LogLevel::Error, "Cannot set sampler on a non-sampler descriptor set");
    return;
  }

  SamplerDx12* samplerDx12 = dynamic_cast<SamplerDx12*>(sampler);
  if (!samplerDx12) {
    GlobalLogger::Log(LogLevel::Error, "Invalid sampler type");
    return;
  }
  if (!samplerDx12->isValid()) {
    GlobalLogger::Log(LogLevel::Error, "Invalid sampler descriptor");
    return;
  }

  auto*    frameResMgr  = m_device_->getFrameResourcesManager();
  uint32_t currentFrame = frameResMgr->getCurrentFrameIndex();
  if (m_samplerIndices_[currentFrame] == UINT32_MAX) {
    uint32_t totalSmp               = m_layout_->getTotalDescriptors();
    m_samplerIndices_[currentFrame] = frameResMgr->getSamplerHeap(currentFrame)->allocate(totalSmp);
  }

  DescriptorHeapDx12* cpuHeap = m_device_->getCpuSamplerHeap();
  DescriptorHeapDx12* gpuHeap = frameResMgr->getCurrentSamplerHeap();
  if (!cpuHeap || !gpuHeap) {
    GlobalLogger::Log(LogLevel::Error, "Descriptor heaps not available");
    return;
  }

  uint32_t srcIndex = samplerDx12->getDescriptorIndex();

  uint32_t bindingOffset = findBindingOffset_(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, binding);
  uint32_t dstIndex      = m_samplerIndices_[currentFrame] + bindingOffset;

  gpuHeap->copyDescriptors(cpuHeap, srcIndex, dstIndex, 1);
}

void DescriptorSetDx12::setStorageBuffer(uint32_t binding, Buffer* buffer, uint64_t offset, uint64_t range) {
  if (!buffer) {
    GlobalLogger::Log(LogLevel::Error, "Null buffer");
    return;
  }

  DescriptorBufferDx12* bufferDx12 = dynamic_cast<DescriptorBufferDx12*>(buffer);
  if (!bufferDx12) {
    GlobalLogger::Log(LogLevel::Error, "Invalid buffer type");
    return;
  }

  auto*    frameResMgr  = m_device_->getFrameResourcesManager();
  uint32_t currentFrame = frameResMgr->getCurrentFrameIndex();
  if (m_srvUavCbvIndices_[currentFrame] == UINT32_MAX) {
    uint32_t totalDesc                = m_layout_->getTotalDescriptors();
    m_srvUavCbvIndices_[currentFrame] = frameResMgr->getCbvSrvUavHeap(currentFrame)->allocate(totalDesc);
  }

  DescriptorHeapDx12* cpuHeap = m_device_->getCpuCbvSrvUavHeap();
  DescriptorHeapDx12* gpuHeap = frameResMgr->getCurrentCbvSrvUavHeap();
  if (!cpuHeap || !gpuHeap) {
    GlobalLogger::Log(LogLevel::Error, "Descriptor heaps not available");
    return;
  }

  uint32_t                    srcIndex;
  D3D12_DESCRIPTOR_RANGE_TYPE rangeType;

  if (bufferDx12->isUnorderedAccessBuffer() && bufferDx12->hasUavHandle()) {
    srcIndex
        = uint32_t((bufferDx12->getUavCpuHandle().ptr - cpuHeap->getCpuHandle(0).ptr) / cpuHeap->getDescriptorSize());
    rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  } else if (bufferDx12->isShaderResourceBuffer() && bufferDx12->hasSrvHandle()) {
    srcIndex
        = uint32_t((bufferDx12->getSrvCpuHandle().ptr - cpuHeap->getCpuHandle(0).ptr) / cpuHeap->getDescriptorSize());
    rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  } else {
    GlobalLogger::Log(LogLevel::Error, "Buffer must have either UAV or SRV capability for storage buffer usage");
    return;
  }

  uint32_t bindingOffset = findBindingOffset_(rangeType, binding);
  uint32_t dstIndex      = m_srvUavCbvIndices_[currentFrame] + bindingOffset;

  gpuHeap->copyDescriptors(cpuHeap, srcIndex, dstIndex, 1);
}
[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE DescriptorSetDx12::getGpuSrvUavCbvHandle(uint32_t frame) const {
  auto* frameResMgr = m_device_->getFrameResourcesManager();
  auto* heap        = frameResMgr->getCbvSrvUavHeap(frame);

  if (!heap) {
    return D3D12_GPU_DESCRIPTOR_HANDLE{0};
  }

  if (m_srvUavCbvIndices_[frame] == UINT32_MAX) {
    GlobalLogger::Log(LogLevel::Error, "No valid descriptor set allocated for this frame");
    return D3D12_GPU_DESCRIPTOR_HANDLE{0};
  }

  return heap->getGpuHandle(m_srvUavCbvIndices_[frame]);
}

[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE DescriptorSetDx12::getGpuSamplerHandle(uint32_t frame) const {
  auto* frameResMgr = m_device_->getFrameResourcesManager();
  auto* heap        = frameResMgr->getSamplerHeap(frame);

  if (!heap) {
    return D3D12_GPU_DESCRIPTOR_HANDLE{0};
  }

  if (m_samplerIndices_[frame] == UINT32_MAX) {
    GlobalLogger::Log(LogLevel::Error, "No valid sampler descriptor allocated for this frame");
    return D3D12_GPU_DESCRIPTOR_HANDLE{0};
  }

  return heap->getGpuHandle(m_samplerIndices_[frame]);
}

uint32_t DescriptorSetDx12::findBindingOffset_(D3D12_DESCRIPTOR_RANGE_TYPE rangeType, uint32_t binding) const {
  const auto& bindingsByType = m_layout_->getBindingsByType();

  std::vector<D3D12_DESCRIPTOR_RANGE_TYPE> rangeOrder;
  if (m_layout_->isSamplerLayout()) {
    rangeOrder = {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER};
  } else {
    // the order: really important! this is because it should perfectly align with descriptor table layout in pipeline
    rangeOrder = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_DESCRIPTOR_RANGE_TYPE_CBV};
  }

  uint32_t offset = 0;
  for (auto type : rangeOrder) {
    auto it = bindingsByType.find(type);
    if (it == bindingsByType.end()) {
      continue;
    }

    const auto& list = it->second;
    if (type == rangeType) {
      // found the type we need, now iterate till we find the binding
      for (const auto& desc : list) {
        // desc.descriptorCount represents how many identical descriptors occupy consecutive slots for this binding
        for (uint32_t i = 0; i < desc.descriptorCount; ++i) {
          if (desc.binding + i == binding) {
            return offset;
          }
          ++offset;
        }
      }
      GlobalLogger::Log(LogLevel::Error, "Binding not found: " + std::to_string(binding));
      return UINT32_MAX;
    } else {
      for (const auto& desc : list) {
        offset += desc.descriptorCount;
      }
    }
  }

  GlobalLogger::Log(LogLevel::Error, "Range type not in layout: " + std::to_string(rangeType));
  return UINT32_MAX;
}

//-------------------------------------------------------------------------
// DescriptorHeapDx12 implementation
//-------------------------------------------------------------------------

DescriptorHeapDx12::~DescriptorHeapDx12() {
  release();
}

bool DescriptorHeapDx12::initialize(ID3D12Device*              device,
                                    D3D12_DESCRIPTOR_HEAP_TYPE type,
                                    uint32_t                   count,
                                    bool                       shaderVisible) {
  if (!device || count == 0) {
    GlobalLogger::Log(LogLevel::Error, "Invalid parameters for descriptor heap initialization");
    return false;
  }

  release();

  std::lock_guard<std::mutex> lock(m_heapMutex);

  m_device        = device;
  m_type          = type;
  m_capacity      = count;
  m_shaderVisible = shaderVisible;
  m_freeList.assign(count, true);

  D3D12_DESCRIPTOR_HEAP_DESC desc{};
  desc.Type           = type;
  desc.NumDescriptors = count;
  desc.Flags          = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  desc.NodeMask       = 0;

  HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
  if (FAILED(hr)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create descriptor heap");
    return false;
  }

  m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
  return true;
}

void DescriptorHeapDx12::release() {
  std::lock_guard<std::mutex> lock(m_heapMutex);
  m_heap.Reset();
  m_freeList.clear();
  m_device         = nullptr;
  m_capacity       = 0;
  m_allocated      = 0;
  m_descriptorSize = 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapDx12::getCpuHandle(uint32_t index) const {
  if (!m_heap || index >= m_capacity) {
    GlobalLogger::Log(LogLevel::Error, "Invalid index for descriptor heap CPU handle");
    return D3D12_CPU_DESCRIPTOR_HANDLE{0};
  }

  D3D12_CPU_DESCRIPTOR_HANDLE handle  = m_heap->GetCPUDescriptorHandleForHeapStart();
  handle.ptr                         += static_cast<SIZE_T>(index) * m_descriptorSize;
  return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapDx12::getGpuHandle(uint32_t index) const {
  if (!m_heap || !m_shaderVisible || index >= m_capacity) {
    GlobalLogger::Log(LogLevel::Error, "Invalid index for descriptor heap GPU handle");
    return D3D12_GPU_DESCRIPTOR_HANDLE{0};
  }

  D3D12_GPU_DESCRIPTOR_HANDLE handle  = m_heap->GetGPUDescriptorHandleForHeapStart();
  handle.ptr                         += static_cast<SIZE_T>(index) * m_descriptorSize;
  return handle;
}

uint32_t DescriptorHeapDx12::allocate(uint32_t count) {
  std::lock_guard<std::mutex> lock(m_heapMutex);
  if (!m_heap || count == 0 || count > m_capacity) {
    GlobalLogger::Log(LogLevel::Warning, "Invalid allocation request for descriptor heap");
    return UINT32_MAX;
  }

  // Find a contiguous block of free descriptors and occupy it
  for (uint32_t i = 0; i <= m_capacity - count; ++i) {
    bool blockAvailable = true;

    for (uint32_t j = 0; j < count; ++j) {
      if (!m_freeList[i + j]) {
        blockAvailable = false;
        break;
      }
    }

    if (blockAvailable) {
      for (uint32_t j = 0; j < count; ++j) {
        m_freeList[i + j] = false;
      }

      m_allocated += count;
      return i;
    }
  }

  GlobalLogger::Log(LogLevel::Warning, "Failed to find space in descriptor heap for allocation");
  return UINT32_MAX;
}

void DescriptorHeapDx12::free(uint32_t index) {
  freeBlock(index, 1);
}

void DescriptorHeapDx12::freeBlock(uint32_t baseIndex, uint32_t count) {
  std::lock_guard<std::mutex> lock(m_heapMutex);
  if (!m_heap || baseIndex + count > m_capacity) {
    return;
  }

  for (uint32_t j = 0; j < count; ++j) {
    if (!m_freeList[baseIndex + j]) {
      m_freeList[baseIndex + j] = true;
      --m_allocated;
    }
  }
}

void DescriptorHeapDx12::copyDescriptors(const DescriptorHeapDx12* srcHeap,
                                         uint32_t                  srcIndex,
                                         uint32_t                  dstIndex,
                                         uint32_t                  count) {
  std::lock_guard<std::mutex> lock(m_heapMutex);
  if (!m_heap || !srcHeap || !srcHeap->getHeap() || srcIndex + count > srcHeap->getCapacity()
      || dstIndex + count > m_capacity) {
    GlobalLogger::Log(LogLevel::Error, "Invalid parameters for descriptor heap copy");
    return;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = srcHeap->getCpuHandle(srcIndex);
  D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = getCpuHandle(dstIndex);

  m_device->CopyDescriptorsSimple(count, dstHandle, srcHandle, m_type);
}

// -------------------------------------------------------------------------
// FrameResourcesManager implementation
// -------------------------------------------------------------------------

// TODO: Make these sizes configurable
constexpr uint32_t FRAME_CBV_SRV_UAV_HEAP_SIZE = 2048;
constexpr uint32_t FRAME_SAMPLER_HEAP_SIZE     = 128;

FrameResourcesManager::~FrameResourcesManager() {
  release();
}

bool FrameResourcesManager::initialize(DeviceDx12* device, uint32_t frameCount) {
  if (!device || frameCount == 0) {
    GlobalLogger::Log(LogLevel::Error, "Invalid parameters for frame resources manager initialization");
    return false;
  }

  release();

  m_device       = device;
  m_frameCount   = frameCount;
  m_currentFrame = 0;

  m_cbvSrvUavHeaps.resize(frameCount);
  for (uint32_t i = 0; i < frameCount; ++i) {
    m_cbvSrvUavHeaps[i] = std::make_unique<DescriptorHeapDx12>();
    if (!m_cbvSrvUavHeaps[i]->initialize(
            device->getDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, FRAME_CBV_SRV_UAV_HEAP_SIZE, true)) {
      GlobalLogger::Log(LogLevel::Error, "Failed to create frame CBV/SRV/UAV heap");
      return false;
    }
  }

  m_samplerHeaps.resize(frameCount);
  for (uint32_t i = 0; i < frameCount; ++i) {
    m_samplerHeaps[i] = std::make_unique<DescriptorHeapDx12>();
    if (!m_samplerHeaps[i]->initialize(
            device->getDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, FRAME_SAMPLER_HEAP_SIZE, true)) {
      GlobalLogger::Log(LogLevel::Error, "Failed to create frame Sampler heap");
      return false;
    }
  }

  GlobalLogger::Log(LogLevel::Info,
                    "Frame resources manager initialized with " + std::to_string(frameCount) + " frames");
  return true;
}

void FrameResourcesManager::release() {
  m_cbvSrvUavHeaps.clear();
  m_samplerHeaps.clear();
  m_device       = nullptr;
  m_frameCount   = 0;
  m_currentFrame = 0;
}

DescriptorHeapDx12* FrameResourcesManager::getCurrentCbvSrvUavHeap() {
  std::lock_guard<std::mutex> lock(m_frameResourcesMutex);
  if (m_currentFrame < m_cbvSrvUavHeaps.size()) {
    return m_cbvSrvUavHeaps[m_currentFrame].get();
  }
  GlobalLogger::Log(LogLevel::Error, "Current frame index exceeds frame count");
  return nullptr;
}

DescriptorHeapDx12* FrameResourcesManager::getCurrentSamplerHeap() {
  std::lock_guard<std::mutex> lock(m_frameResourcesMutex);
  if (m_currentFrame < m_samplerHeaps.size()) {
    return m_samplerHeaps[m_currentFrame].get();
  }
  GlobalLogger::Log(LogLevel::Error, "Current frame index exceeds frame count");
  return nullptr;
}

inline DescriptorHeapDx12* FrameResourcesManager::getCbvSrvUavHeap(uint32_t frameIndex) {
  std::lock_guard<std::mutex> lock(m_frameResourcesMutex);
  if (frameIndex >= m_frameCount) {
    GlobalLogger::Log(
        LogLevel::Error,
        "Frame index " + std::to_string(frameIndex) + " exceeds frame count " + std::to_string(m_frameCount));
    return nullptr;
  }
  return m_cbvSrvUavHeaps[frameIndex].get();
}

inline DescriptorHeapDx12* FrameResourcesManager::getSamplerHeap(uint32_t frameIndex) {
  std::lock_guard<std::mutex> lock(m_frameResourcesMutex);
  if (frameIndex >= m_frameCount) {
    GlobalLogger::Log(
        LogLevel::Error,
        "Frame index " + std::to_string(frameIndex) + " exceeds frame count " + std::to_string(m_frameCount));
    return nullptr;
  }
  return m_samplerHeaps[frameIndex].get();
}

void FrameResourcesManager::nextFrame() {
  std::lock_guard<std::mutex> lock(m_frameResourcesMutex);
  if (m_frameCount > 0) {
    m_currentFrame = (m_currentFrame + 1) % m_frameCount;
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise
#endif  // ARISE_USE_DX12
