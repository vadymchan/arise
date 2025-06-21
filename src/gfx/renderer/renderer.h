#ifndef ARISE_RENDERER_H
#define ARISE_RENDERER_H

#include "gfx/renderer/frame_resources.h"
#include "gfx/renderer/passes/base_pass.h"
#include "gfx/renderer/passes/debug_pass.h"
#include "gfx/renderer/passes/final_pass.h"
#include "gfx/renderer/render_resource_manager.h"
#include "gfx/rhi/common/rhi_enums.h"
#include "gfx/rhi/interface/command_buffer.h"
#include "gfx/rhi/interface/device.h"
#include "gfx/rhi/interface/swap_chain.h"
#include "gfx/rhi/interface/synchronization.h"
#include "gfx/rhi/shader_manager.h"

#include <memory>

namespace arise {
class Scene;
}  // namespace arise

namespace arise {
namespace gfx {
namespace renderer {

/**
 * Coordinates the rendering process, manages frame synchronization
 */
class Renderer {
  public:
  Renderer() = default;

  ~Renderer() { waitForAllFrames_(); }

  bool initialize(Window* window, rhi::RenderingApi api);

  RenderContext beginFrame(Scene* scene, const RenderSettings& renderSettings);
  void          renderFrame(RenderContext& context);
  void          endFrame(RenderContext& context);

  bool onWindowResize(uint32_t width, uint32_t height);
  bool onViewportResize(const math::Dimension2i& newDimension);
  void onSceneSwitch();

  rhi::Device*           getDevice() const { return m_device.get(); }
  rhi::ShaderManager*    getShaderManager() const { return m_shaderManager.get(); }
  FrameResources*        getFrameResources() const { return m_frameResources.get(); }
  RenderResourceManager* getResourceManager() const { return m_resourceManager.get(); }
  gfx::rhi::RenderingApi getCurrentApi() const;

  void updateWindow(Window* window);

  bool switchRenderingApi(gfx::rhi::RenderingApi newApi);

  private:
  void initializeGpuProfiler_();

  std::unique_ptr<rhi::CommandBuffer> acquireCommandBuffer_();

  void recycleCommandBuffer_(std::unique_ptr<rhi::CommandBuffer> cmdBuffer);

  void waitForAllFrames_();

  void setupRenderPasses_();

  void cleanupResources_();
  bool recreateResources_(gfx::rhi::RenderingApi newApi);
  void recreateResourceManagers_();
  void reloadSceneModels_();

  static constexpr uint32_t COMMAND_BUFFERS_PER_FRAME = 8;
  static constexpr uint32_t INITIAL_COMMAND_BUFFERS   = 2;

  Window*                                m_window = nullptr;
  std::unique_ptr<rhi::Device>           m_device;
  std::unique_ptr<rhi::SwapChain>        m_swapChain;
  std::unique_ptr<rhi::ShaderManager>    m_shaderManager;
  std::unique_ptr<RenderResourceManager> m_resourceManager;
  std::unique_ptr<FrameResources>        m_frameResources;

  std::unique_ptr<BasePass>  m_basePass;
  std::unique_ptr<FinalPass> m_finalPass;
  std::unique_ptr<DebugPass> m_debugPass;

  // one pool per frame in flight
  std::vector<std::vector<std::unique_ptr<rhi::CommandBuffer>>> m_commandBufferPools;

  std::vector<std::unique_ptr<rhi::Fence>>     m_frameFences;
  std::vector<std::unique_ptr<rhi::Semaphore>> m_imageAvailableSemaphores;
  std::vector<std::unique_ptr<rhi::Semaphore>> m_renderFinishedSemaphores;

  bool m_initialized = false;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_RENDERER_H