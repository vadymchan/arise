#ifndef ARISE_COMMAND_BUFFER_DX12_H
#define ARISE_COMMAND_BUFFER_DX12_H

#include "gfx/rhi/interface/command_buffer.h"
#include "platform/windows/windows_platform_setup.h"

#ifdef ARISE_USE_DX12

namespace arise {
namespace gfx {
namespace rhi {

class DeviceDx12;
class BufferDx12;
class TextureDx12;
class DescriptorSetDx12;
class GraphicsPipelineDx12;
class RenderPassDx12;
class FramebufferDx12;

// clang-format off

/**
 * @note This class holds a reference to its command allocator, which simplifies the design for educational purposes.
 *       This approach makes the class self-contained and easier for students to understand by mirroring the actual
 * dependency between the command buffer and its allocator in the DirectX 12 API.
 *
 * @note Although DirectX 12 typically refers to its command buffer as a "Command List", the name "CommandBufferDx12"
 *       was chosen to maintain consistency with the Vulkan implementation and to emphasize the concept of a
 *       self-contained command buffer that encapsulates its command allocator.
 */
class CommandBufferDx12 : public CommandBuffer {
  public:
  CommandBufferDx12(DeviceDx12* device, ComPtr<ID3D12GraphicsCommandList> commandList, ID3D12CommandAllocator* commandAllocator);
  ~CommandBufferDx12();

  // Command buffer recording
  void begin() override;
  void end() override;
  void reset() override;

  // Pipeline state
  void setPipeline(Pipeline* pipeline) override;
  void setViewport(const Viewport& viewport) override;
  void setScissor(const ScissorRect& scissor) override;

  // Resource binding
  void bindVertexBuffer(uint32_t binding, Buffer* buffer, uint64_t offset = 0) override;
  void bindIndexBuffer(Buffer* buffer, uint64_t offset = 0, bool use32BitIndices = false) override;
  // @param rootParameterIndex: Root parameter index as defined in the root signature. 
  // @note  descriptor set should be compatible with the pipeline's root signature.
  void bindDescriptorSet(uint32_t rootParameterIndex, DescriptorSet* set) override;

  // Draw commands
  void draw(uint32_t vertexCount, uint32_t firstVertex = 0) override;
  void drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0) override;
  void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;
  void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) override;
  
  // Resource barriers
  void resourceBarrier(const ResourceBarrierDesc& barrier) override;

  // Render pass operations
  /**
   * @note DirectX 12 doesn't have the concept of render passes like Vulkan. These methods emulate Vulkan's render pass 
   *       behavior by managing render target binding, clearing, and resource state transitions to provide a unified 
   *       API across backends.
   */
  void beginRenderPass(RenderPass* renderPass, Framebuffer* framebuffer, const std::vector<ClearValue>& clearValues) override;
  void endRenderPass() override;

  // Copy operations
  void copyBuffer(Buffer* srcBuffer, Buffer* dstBuffer, uint64_t srcOffset = 0, uint64_t dstOffset = 0, uint64_t size = 0) override;
  void copyBufferToTexture(Buffer* srcBuffer, Texture* dstTexture, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) override;
  void copyTextureToBuffer(Texture* srcTexture, Buffer* dstBuffer, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) override;
  void copyTexture(Texture* srcTexture, Texture* dstTexture, uint32_t srcMipLevel   = 0, uint32_t srcArrayLayer = 0, uint32_t dstMipLevel   = 0, uint32_t dstArrayLayer = 0) override;

  // Clear operations
  void clearColor(Texture* texture, const float color[4], uint32_t mipLevel = 0, uint32_t arrayLayer = 0) override;
  void clearDepthStencil(Texture* texture, float depth, uint8_t stencil, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) override;

  // Debug markers
  void beginDebugMarker(const std::string& name, const float color[4] = nullptr) override;
  void endDebugMarker() override;
  void insertDebugMarker(const std::string& name, const float color[4] = nullptr) override;

  // DX12-specific methods
  ID3D12GraphicsCommandList* getCommandList() const { return m_commandList_.Get(); }

  ID3D12CommandAllocator* getCommandAllocator() const { return m_commandAllocator_; }

  /**
   * Binds the GPU descriptor heaps (CBV/SRV/UAV heap and Sampler heap)
   * 
   * @note This is a simplified implementation that automatically binds the current
   *       frame's descriptor heaps. If you need more advanced descriptor heap binding logic,
   *       consider modifying this method or explicitly managing heaps elsewhere.
   */
  void bindDescriptorHeaps();

  private:
  DeviceDx12*                m_device_;
  ComPtr<ID3D12GraphicsCommandList> m_commandList_;
  ID3D12CommandAllocator*    m_commandAllocator_;

  GraphicsPipelineDx12*    m_currentPipeline_    = nullptr;
  RenderPassDx12*          m_currentRenderPass_  = nullptr;
  FramebufferDx12*         m_currentFramebuffer_ = nullptr;
  bool                     m_isRenderPassActive_ = false;
  bool                     m_isRecording_        = false;
};

// clang-format on

class CommandAllocatorManager {
  public:
  CommandAllocatorManager() = default;
  ~CommandAllocatorManager();

  bool initialize(ID3D12Device* device, uint32_t allocatorCount = 8);
  void release();

  ID3D12CommandAllocator* getCommandAllocator();

  void returnCommandAllocator(ID3D12CommandAllocator* allocator);

  private:
  ID3D12Device*                               m_device = nullptr;
  std::vector<ComPtr<ID3D12CommandAllocator>> m_availableAllocators;
  std::vector<ComPtr<ID3D12CommandAllocator>> m_usedAllocators;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12
#endif  // ARISE_COMMAND_BUFFER_DX12_H