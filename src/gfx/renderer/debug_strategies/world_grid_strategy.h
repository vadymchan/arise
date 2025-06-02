#ifndef ARISE_WORLD_GRID_STRATEGY_H
#define ARISE_WORLD_GRID_STRATEGY_H

#include "gfx/renderer/debug_strategies/debug_draw_strategy.h"
#include "gfx/rhi/interface/render_pass.h"

#include <memory>
#include <vector>

namespace arise {
namespace gfx {
namespace renderer {

/**
 * Renders an infinite grid on the Y=0 plane
 * Uses a fullscreen pass with ray-plane intersection
 */
class WorldGridStrategy : public DebugDrawStrategy {
  public:
  WorldGridStrategy() = default;
  ~WorldGridStrategy() override { cleanup(); }

  void initialize(rhi::Device*           device,
                  RenderResourceManager* resourceManager,
                  FrameResources*        frameResources,
                  rhi::ShaderManager*    shaderManager) override;

  void resize(const math::Dimension2Di& newDimension) override;
  void prepareFrame(const RenderContext& context) override;
  void render(const RenderContext& context) override;
  void cleanup() override;

  bool isExclusive() const override { return false; }

  private:
  //struct GridParameters {
  //  math::Matrix4f<> invViewProjection;
  //  math::Vector3Df  cameraPosition;
  //  float            gridSize;
  //  float            fadeDistance;
  //  float            padding[2];
  //};

  void setupRenderPass_();
  void createFramebuffers_(const math::Dimension2Di& dimension);
  void createPipeline_();
  //void updateGridParameters_(const RenderContext& context);

  const std::string m_vertexShaderPath_ = "assets/shaders/debug/world_grid/simple_grid.vs.hlsl";
  const std::string m_pixelShaderPath_  = "assets/shaders/debug/world_grid/simple_grid.ps.hlsl";

  rhi::Device*           m_device          = nullptr;
  RenderResourceManager* m_resourceManager = nullptr;
  FrameResources*        m_frameResources  = nullptr;
  rhi::ShaderManager*    m_shaderManager   = nullptr;

  rhi::Shader*           m_vertexShader = nullptr;
  rhi::Shader*           m_pixelShader  = nullptr;
  rhi::GraphicsPipeline* m_pipeline     = nullptr;

  rhi::Viewport    m_viewport;
  rhi::ScissorRect m_scissor;

  rhi::RenderPass*               m_renderPass = nullptr;
  std::vector<rhi::Framebuffer*> m_framebuffers;

  //rhi::Buffer*              m_gridParametersBuffer = nullptr;
  //rhi::DescriptorSetLayout* m_gridLayout           = nullptr;
  //rhi::DescriptorSet*       m_gridDescriptorSet    = nullptr;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_WORLD_GRID_STRATEGY_H