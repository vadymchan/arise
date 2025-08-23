#ifndef ARISE_FINAL_PASS_H
#define ARISE_FINAL_PASS_H

#include "gfx/renderer/render_pass.h"

namespace arise {
namespace gfx {
namespace renderer {

/**
 * Copies from the color buffer to the back buffer
 */
class FinalPass : public RenderPass {
  public:
  FinalPass() = default;

  ~FinalPass() override { cleanup(); }

  void initialize(rhi::Device*           device,
                  RenderResourceManager* resourceManager,
                  FrameResources*        frameResources,
                  rhi::ShaderManager*    shaderManager) override;

  void resize(const math::Dimension2i& newDimension) override {}

  void prepareFrame(const RenderContext& context) override {}

  void render(RenderContext& context) override;

  void cleanup() override;

  private:
  rhi::Device*           m_device          = nullptr;
  RenderResourceManager* m_resourceManager = nullptr;
  FrameResources*        m_frameResources  = nullptr;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_FINAL_PASS_H