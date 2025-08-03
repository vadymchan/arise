#ifndef ARISE_RHI_DEVICE_H
#define ARISE_RHI_DEVICE_H

#include "gfx/rhi/common/rhi_enums.h"
#include "gfx/rhi/common/rhi_types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace arise {
class Window;

namespace gfx {
namespace rhi {

class Buffer;
class Texture;
class Sampler;
class Shader;
class GraphicsPipeline;
class DescriptorSetLayout;
class DescriptorSet;
class RenderPass;
class Framebuffer;
class CommandBuffer;
class Fence;
class Semaphore;
class SwapChain;

// clang-format off

/**
 * Main RHI device interface
 *
 * The Device represents the graphics API device.
 * It manages GPU resources and handles command submission.
 */
class Device {
  public:
  Device(const DeviceDesc& desc)
      : m_window_(desc.window) {}

  virtual ~Device() = default;

  virtual RenderingApi getApiType() const = 0;

  const Window* getWindow() const { return m_window_; }

  virtual std::unique_ptr<Buffer>              createBuffer(const BufferDesc& desc)                                     = 0;
  virtual std::unique_ptr<Texture>             createTexture(const TextureDesc& desc)                                   = 0;
  virtual std::unique_ptr<Sampler>             createSampler(const SamplerDesc& desc)                                   = 0;
  virtual std::unique_ptr<Shader>              createShader(const ShaderDesc& desc)                                     = 0;
  virtual std::unique_ptr<GraphicsPipeline>    createGraphicsPipeline(const GraphicsPipelineDesc& desc)                 = 0;
  /**
   * Create graphics pipeline with automatic shader reflection
   */
  virtual std::unique_ptr<GraphicsPipeline>    createGraphicsPipelineWithReflection(const GraphicsPipelineDesc& desc) = 0;
  virtual std::unique_ptr<DescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc)           = 0;
  virtual std::unique_ptr<DescriptorSet>       createDescriptorSet(const DescriptorSetLayout* layout)                   = 0;
  virtual std::unique_ptr<RenderPass>          createRenderPass(const RenderPassDesc& desc)                             = 0;
  virtual std::unique_ptr<Framebuffer>         createFramebuffer(const FramebufferDesc& desc)                           = 0;
  virtual std::unique_ptr<CommandBuffer>       createCommandBuffer(const CommandBufferDesc& desc = CommandBufferDesc()) = 0;
  virtual std::unique_ptr<Fence>               createFence(const FenceDesc& desc = FenceDesc())                         = 0;
  virtual std::unique_ptr<Semaphore>           createSemaphore()                                                        = 0;
  virtual std::unique_ptr<SwapChain>           createSwapChain(const SwapchainDesc& desc)                               = 0;

  virtual void updateBuffer(Buffer* buffer, const void* data, size_t size, size_t offset = 0)                                     = 0;
  virtual void updateTexture(Texture* texture, const void* data, size_t dataSize, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) = 0;

  /**
   * @param cmdBuffer The command buffer to submit. MUST be in the "closed" state (end() must have been called prior to this method)
   */
  virtual void submitCommandBuffer(CommandBuffer* cmdBuffer, Fence* signalFence = nullptr, const std::vector<Semaphore*>& waitSemaphores = {}, const std::vector<Semaphore*>& signalSemaphores = {}) = 0;

  virtual void waitIdle() = 0;

  private:
  // TODO: change constness if needed
  const Window* const m_window_;
};

// clang-format on

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_RHI_DEVICE_H