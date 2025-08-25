#ifndef ARISE_COMMAND_BUFFER_H
#define ARISE_COMMAND_BUFFER_H

#include "gfx/rhi/common/rhi_enums.h"
#include "gfx/rhi/common/rhi_types.h"

namespace arise {
namespace gfx {
namespace rhi {

class Pipeline;
class Buffer;
class Texture;
class DescriptorSet;
class RenderPass;
class Framebuffer;

// clang-format off

/**
 * Represents a buffer to record rendering commands for the GPU.
 * Used to submit draw, compute, and transfer operations.
 */
class CommandBuffer {
  public:
  CommandBuffer()          = default;
  virtual ~CommandBuffer() = default;

  // Command buffer recording
  virtual void begin() = 0;
  virtual void end()   = 0;
  virtual void reset() = 0;

  // Pipeline state
  virtual void setPipeline(Pipeline* pipeline)        = 0;
  virtual void setViewport(const Viewport& viewport)  = 0;
  virtual void setScissor(const ScissorRect& scissor) = 0;

  // Resource binding
  virtual void bindVertexBuffer(uint32_t binding, Buffer* buffer, uint64_t offset = 0)            = 0;
  virtual void bindIndexBuffer(Buffer* buffer, uint64_t offset = 0, bool use32BitIndices = false) = 0;
  virtual void bindDescriptorSet(uint32_t setIndex, DescriptorSet* set)                           = 0;

  // Draw commands
  virtual void draw(uint32_t vertexCount, uint32_t firstVertex = 0)                                                                                             = 0;
  virtual void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex = 0, uint32_t firstInstance = 0)                                = 0;
  virtual void drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0)                                                              = 0;
  virtual void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) = 0;
  // TODO: add more draw commands if needed

  // Compute dispatch
  virtual void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

  // Resource barriers
  virtual void resourceBarrier(const ResourceBarrierDesc& barrier) = 0;

  // Render pass operations
  virtual void beginRenderPass(RenderPass* renderPass, Framebuffer* framebuffer, const std::vector<ClearValue>& clearValues) = 0;
  virtual void endRenderPass()                                                                                               = 0;

  // Copy operations
  virtual void copyBuffer(Buffer* srcBuffer, Buffer* dstBuffer, uint64_t srcOffset = 0, uint64_t dstOffset = 0, uint64_t size = 0)														   = 0;
  virtual void copyBufferToTexture(Buffer* srcBuffer, Texture* dstTexture, uint32_t mipLevel = 0, uint32_t arrayLayer = 0)																   = 0;
  virtual void copyTextureToBuffer(Texture* srcTexture, Buffer* dstBuffer, uint32_t mipLevel = 0, uint32_t arrayLayer = 0)																   = 0;
  virtual void copyTexture(Texture*  srcTexture, Texture*  dstTexture, uint32_t  srcMipLevel   = 0, uint32_t  srcArrayLayer = 0, uint32_t  dstMipLevel   = 0, uint32_t  dstArrayLayer = 0) = 0;

  // Clear operations
  // TODO: seems that currently not used (consider remove)
  virtual void clearColor(Texture* texture, const float color[4], uint32_t mipLevel = 0, uint32_t arrayLayer = 0)                = 0;
  virtual void clearDepthStencil(Texture* texture, float depth, uint8_t stencil, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) = 0;

  // Debug markers
  virtual void beginDebugMarker(const std::string& name, const float color[4] = nullptr)  = 0;
  virtual void endDebugMarker()                                                           = 0;
  virtual void insertDebugMarker(const std::string& name, const float color[4] = nullptr) = 0;
};

// clang-format on

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_COMMAND_BUFFER_H