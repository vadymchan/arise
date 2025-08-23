#ifndef ARISE_DEBUG_PASS_H
#define ARISE_DEBUG_PASS_H

#include "gfx/renderer/debug_strategies/debug_draw_strategy.h"
#include "gfx/renderer/render_pass.h"

#include <memory>

namespace arise {
namespace gfx {
namespace renderer {

/**
 * Handles debug visualization rendering
 */
class DebugPass : public RenderPass {
  public:
  DebugPass();
  ~DebugPass() override;

  void initialize(rhi::Device*           device,
                  RenderResourceManager* resourceManager,
                  FrameResources*        frameResources,
                  rhi::ShaderManager*    shaderManager) override;

  void resize(const math::Dimension2i& newDimension) override;
  void prepareFrame(const RenderContext& context) override;
  void render(RenderContext& context) override;

  void endFrame() override {}
  void clearSceneResources();
  void cleanup() override;

  bool isExclusive() const override;

  private:
  void createDebugStrategy_();

  rhi::Device*           m_device          = nullptr;
  RenderResourceManager* m_resourceManager = nullptr;
  FrameResources*        m_frameResources  = nullptr;
  rhi::ShaderManager*    m_shaderManager   = nullptr;

  std::unique_ptr<DebugDrawStrategy> m_debugStrategy;
  RenderMode                         m_currentRenderMode = RenderMode::Solid;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_DEBUG_PASS_H