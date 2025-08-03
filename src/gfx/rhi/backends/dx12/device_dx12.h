#ifndef ARISE_DEVICE_DX12_H
#define ARISE_DEVICE_DX12_H

#include "gfx/rhi/backends/dx12/command_buffer_dx12.h"
#include "gfx/rhi/backends/dx12/descriptor_dx12.h"
#include "gfx/rhi/interface/device.h"
#include "platform/windows/windows_platform_setup.h"

#include <D3D12MemAlloc.h>

#include <mutex>

#ifdef ARISE_USE_DX12

namespace arise {
namespace gfx {
namespace rhi {

class DeviceDx12 : public Device {
  public:
  DeviceDx12(const DeviceDesc& desc);
  ~DeviceDx12() override;

  RenderingApi getApiType() const override { return RenderingApi::Dx12; }

  // clang-format off
  std::unique_ptr<Buffer>              createBuffer(const BufferDesc& desc) override;
  std::unique_ptr<Texture>             createTexture(const TextureDesc& desc) override;
  std::unique_ptr<Sampler>             createSampler(const SamplerDesc& desc) override;
  std::unique_ptr<Shader>              createShader(const ShaderDesc& desc) override;
  std::unique_ptr<GraphicsPipeline>    createGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
  std::unique_ptr<GraphicsPipeline>    createGraphicsPipelineWithReflection(const GraphicsPipelineDesc& desc) override;
  std::unique_ptr<DescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) override;
  std::unique_ptr<DescriptorSet>       createDescriptorSet(const DescriptorSetLayout* layout) override;
  std::unique_ptr<RenderPass>          createRenderPass(const RenderPassDesc& desc) override;
  std::unique_ptr<Framebuffer>         createFramebuffer(const FramebufferDesc& desc) override;
  std::unique_ptr<CommandBuffer>       createCommandBuffer(const CommandBufferDesc& desc = CommandBufferDesc()) override;
  std::unique_ptr<Fence>               createFence(const FenceDesc& desc = FenceDesc()) override;
  std::unique_ptr<Semaphore>           createSemaphore() override;
  std::unique_ptr<SwapChain>           createSwapChain(const SwapchainDesc& desc) override;
  // clang-format on

  void updateBuffer(Buffer* buffer, const void* data, size_t size, size_t offset = 0) override;
  void updateTexture(
      Texture* texture, const void* data, size_t dataSize, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) override;

  /**
   * The command buffer must already be in the "closed" state (end() - ID3D12GraphicsCommandList::Close() must have been
   * called)
   */
  void submitCommandBuffer(CommandBuffer*                 cmdBuffer,
                           Fence*                         signalFence      = nullptr,
                           const std::vector<Semaphore*>& waitSemaphores   = {},
                           const std::vector<Semaphore*>& signalSemaphores = {}) override;

  void waitIdle() override;

  IDXGIFactory6* getFactory() const { return m_factory_.Get(); }

  ID3D12Device* getDevice() const { return m_device_.Get(); }

  D3D12MA::Allocator* getAllocator() const { return m_allocator_.Get(); }

  ID3D12CommandQueue* getCommandQueue() const { return m_commandQueue_.Get(); }

  DescriptorHeapDx12* getCpuRtvHeap() { return &m_cpuRtvHeap; }

  DescriptorHeapDx12* getCpuDsvHeap() { return &m_cpuDsvHeap; }

  FrameResourcesManager* getFrameResourcesManager() { return &m_frameResourcesManager; }

  DescriptorHeapDx12* getCpuCbvSrvUavHeap() { return &m_cpuCbvSrvUavHeap; }

  DescriptorHeapDx12* getCpuSamplerHeap() { return &m_cpuSamplerHeap; }

  void returnCommandAllocator(ID3D12CommandAllocator* allocator) {
    m_commandAllocatorManager_.returnCommandAllocator(allocator);
  }

  private:
  bool createFactory_();
  bool createDevice_();
  bool createCommandQueue_();
  bool createCommandPools_();
  bool createDescriptorHeaps_();
  bool createAllocator_();

  bool findAdapter_(IDXGIAdapter1** adapter);

  ComPtr<IDXGIFactory6> m_factory_;
  ComPtr<IDXGIAdapter3> m_adapter_;
  ComPtr<ID3D12Device>  m_device_;

  ComPtr<ID3D12CommandQueue> m_commandQueue_;
  std::mutex                 m_queueSubmitMutex;

  ComPtr<D3D12MA::Allocator> m_allocator_;

  DescriptorHeapDx12 m_cpuRtvHeap;
  DescriptorHeapDx12 m_cpuDsvHeap;

  DescriptorHeapDx12 m_cpuCbvSrvUavHeap;
  DescriptorHeapDx12 m_cpuSamplerHeap;

  // handles gpu descriptor heaps
  FrameResourcesManager m_frameResourcesManager;

  CommandAllocatorManager m_commandAllocatorManager_;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12

#endif  // ARISE_DEVICE_DX12_H