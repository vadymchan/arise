#ifndef ARISE_IMGUI_RHI_CONTEXT_H
#define ARISE_IMGUI_RHI_CONTEXT_H

#include "gfx/rhi/backends/dx12/descriptor_dx12.h"
#include "gfx/rhi/backends/vulkan/descriptor_vk.h"
#include "gfx/rhi/interface/buffer.h"
#include "gfx/rhi/interface/command_buffer.h"
#include "gfx/rhi/interface/descriptor.h"
#include "gfx/rhi/interface/device.h"
#include "gfx/rhi/interface/framebuffer.h"
#include "gfx/rhi/interface/render_pass.h"
#include "gfx/rhi/interface/sampler.h"
#include "gfx/rhi/interface/texture.h"
#include "platform/common/window.h"

#include <imgui.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace arise {
namespace gfx {

/**
 * Manages ImGui integration with RHI abstraction layer
 */
class ImGuiRHIContext {
  public:
  ImGuiRHIContext() = default;
  ~ImGuiRHIContext();

  bool initialize(Window* window, rhi::Device* device, uint32_t swapChainBufferCount);

  void shutdown();

  void beginFrame();

  void endFrame(rhi::CommandBuffer*       cmdBuffer,
                rhi::Texture*             targetTexture,
                const math::Dimension2i& viewportDimension,
                uint32_t                  currentFrameIndex);

  void resize(const math::Dimension2i& newDimensions);

  ImTextureID createTextureID(rhi::Texture* texture, uint32_t currentIndex);

  void releaseTextureID(ImTextureID textureID);

  private:
  bool initializeVulkan(rhi::Device* device, uint32_t swapChainBufferCount);
  bool initializeDx12(rhi::Device* device, uint32_t swapChainBufferCount);

  void renderImGuiVulkan(rhi::CommandBuffer*       cmdBuffer,
                         rhi::Texture*             targetTexture,
                         const math::Dimension2i& viewportDimension,
                         uint32_t                  currentFrameIndex);

  void renderImGuiDx12(rhi::CommandBuffer*       cmdBuffer,
                       rhi::Texture*             targetTexture,
                       const math::Dimension2i& viewportDimension,
                       uint32_t                  currentFrameIndex);

  void              createRenderPass();
  rhi::Framebuffer* getOrCreateFramebuffer(rhi::Texture*             targetTexture,
                                           const math::Dimension2i& dimensions,
                                           uint32_t                  frameIndex);

  Window*           m_window       = nullptr;
  rhi::Device*      m_device       = nullptr;
  rhi::RenderingApi m_renderingApi = rhi::RenderingApi::Vulkan;

  std::unique_ptr<rhi::RenderPass>               m_renderPass;
  std::vector<std::unique_ptr<rhi::Framebuffer>> m_framebuffers;

  std::unique_ptr<rhi::Sampler> m_sampler;

#ifdef ARISE_USE_VULKAN
  std::unique_ptr<rhi::DescriptorPoolManager> m_imguiPoolManager;
#endif  // ARISE_USE_VULKAN

#ifdef ARISE_USE_DX12
  std::unique_ptr<rhi::DescriptorHeapDx12> m_dx12ImGuiDescriptorHeap;
#endif

  math::Dimension2i m_currentFramebufferSize;

  bool m_initialized = false;
};

}  // namespace gfx
}  // namespace arise

#endif  // ARISE_IMGUI_RHI_CONTEXT_H