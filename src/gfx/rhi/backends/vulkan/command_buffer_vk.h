#ifndef ARISE_COMMAND_BUFFER_VK_H
#define ARISE_COMMAND_BUFFER_VK_H

#include "gfx/rhi/interface/command_buffer.h"

#include <vulkan/vulkan.h>

#include <mutex>

namespace arise {
namespace gfx {
namespace rhi {

class DeviceVk;
class GraphicsPipelineVk;
class ComputePipelineVk;
class BufferVk;
class TextureVk;
class DescriptorSetVk;
class RenderPassVk;
class FramebufferVk;

// clang-format off

/**
 * @note This class holds a reference to its command pool, which simplifies the design for educational purposes.
 *       This approach makes the class self-contained and easier for students to understand by reflecting the actual
 *       dependency between the command buffer and the pool in the Vulkan API.
 */
class CommandBufferVk : public CommandBuffer {
  public:
  CommandBufferVk(DeviceVk* device, VkCommandBuffer commandBuffer, VkCommandPool commandPool);
  ~CommandBufferVk() = default;

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
  // @param setIndex: Descriptor set slot as declared in the shader (e.g. "layout(set = N, ...)" in GLSL).
  void bindDescriptorSet(uint32_t setIndex, DescriptorSet* set) override;

  // Draw commands
  void draw(uint32_t vertexCount, uint32_t firstVertex = 0) override;
  void drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0) override;
  void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;
  void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) override;

  // Compute dispatch
  void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

  // Resource barriers
  void resourceBarrier(const ResourceBarrierDesc& barrier) override;

  // Render pass operations
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

  // Vulkan-specific methods
  const VkCommandBuffer& getCommandBuffer() const { return m_commandBuffer_; }

  private:
  DeviceVk*       m_device_;
  VkCommandBuffer m_commandBuffer_;
  VkCommandPool   m_commandPool_;

  // Current state tracking
  GraphicsPipelineVk* m_currentPipeline_        = nullptr;
  ComputePipelineVk*  m_currentComputePipeline_ = nullptr;
  RenderPassVk*       m_currentRenderPass_      = nullptr;
  FramebufferVk*      m_currentFramebuffer_     = nullptr;
  bool                m_isRenderPassActive_     = false;
  bool                m_isRecording_            = false;
  VkPipelineBindPoint m_currentBindPoint_       = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

// clang-format on

class CommandPoolManager {
  public:
  CommandPoolManager() = default;
  ~CommandPoolManager();

  bool initialize(VkDevice device, uint32_t queueFamilyIndex);
  void release();

  VkCommandPool getPool() const;

  private:
  VkCommandPool createThreadPool() const;

  void registerThreadPool(VkCommandPool pool) const;

  void cleanupThreadPools();

  bool m_isInitialized = false;

  VkDevice      m_device_           = VK_NULL_HANDLE;
  VkCommandPool m_mainCommandPool   = VK_NULL_HANDLE;
  uint32_t      m_queueFamilyIndex_ = 0;

  std::thread::id                                            m_mainThreadId;
  mutable std::mutex                                         m_threadPoolsMutex;
  mutable std::unordered_map<std::thread::id, VkCommandPool> m_threadPools;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_COMMAND_BUFFER_VK_H