#ifndef ARISE_BOUNDING_BOX_VISUALIZATION_STRATEGY_H
#define ARISE_BOUNDING_BOX_VISUALIZATION_STRATEGY_H

#include "gfx/renderer/debug_strategies/debug_draw_strategy.h"

#include <unordered_map>
#include <vector>

namespace arise {
namespace ecs {
struct RenderModel;
struct BoundingBox;
}  // namespace ecs
}  // namespace arise

namespace arise::gfx::rhi {
class Buffer;
class DescriptorSet;
class GraphicsPipeline;
class RenderPass;
}  // namespace arise::gfx::rhi

namespace arise {
namespace gfx {
namespace renderer {

/**
 * Renders bounding boxes as wireframe around models
 */
class BoundingBoxVisualizationStrategy : public DebugDrawStrategy {
  public:
  BoundingBoxVisualizationStrategy() = default;

  ~BoundingBoxVisualizationStrategy() override { cleanup(); }

  void initialize(rhi::Device*           device,
                  RenderResourceManager* resourceManager,
                  FrameResources*        frameResources,
                  rhi::ShaderManager*    shaderManager) override;

  void resize(const math::Dimension2i& newDimension) override;
  void prepareFrame(const RenderContext& context) override;
  void render(const RenderContext& context) override;
  void clearSceneResources() override;
  void cleanup() override;

  bool isExclusive() const override { return false; }

  private:
  struct BoundingBoxData {
    // Local bounds + transform mode (OBB)
    math::Matrix4f<> modelMatrix;
    math::Vector3f   localMinCorner;
    math::Vector3f   localMaxCorner;

    // World bounds mode (AABB)
    math::Vector3f worldMinCorner;
    math::Vector3f worldMaxCorner;
  };

  struct ModelBufferCache {
    rhi::Buffer* instanceBuffer = nullptr;
    uint32_t     capacity       = 0;
    uint32_t     count          = 0;
  };

  struct DrawData {
    rhi::GraphicsPipeline* pipeline                 = nullptr;
    rhi::DescriptorSet*    modelMatrixDescriptorSet = nullptr;
    rhi::Buffer*           instanceBuffer           = nullptr;
    uint32_t               instanceCount            = 0;
  };

  void setupRenderPass_();
  void createFramebuffers_(const math::Dimension2i& dimension);
  void prepareDrawCalls_(const RenderContext& context);
  void updateInstanceBuffer_(ecs::RenderModel*                        model,
                             const std::vector<BoundingBoxData>& boundingBoxData,
                             ModelBufferCache&                   cache);
  void cleanupUnusedBuffers_(
      const std::unordered_map<ecs::RenderModel*, std::vector<BoundingBoxData>>& currentFrameInstances);
  void createBoundingBoxGeometry_();

  const std::string m_vertexShaderPath_ = "assets/shaders/debug/bounding_box/shader_instancing.vs.hlsl";
  const std::string m_pixelShaderPath_  = "assets/shaders/debug/bounding_box/shader.ps.hlsl";

  rhi::Device*           m_device          = nullptr;
  RenderResourceManager* m_resourceManager = nullptr;
  FrameResources*        m_frameResources  = nullptr;
  rhi::ShaderManager*    m_shaderManager   = nullptr;

  rhi::Shader* m_vertexShader = nullptr;
  rhi::Shader* m_pixelShader  = nullptr;

  rhi::Viewport    m_viewport;
  rhi::ScissorRect m_scissor;

  rhi::RenderPass*               m_renderPass = nullptr;
  std::vector<rhi::Framebuffer*> m_framebuffers;

  // Geometry for unit cube wireframe (will be scaled by bounding box)
  rhi::Buffer* m_cubeVertexBuffer = nullptr;
  rhi::Buffer* m_cubeIndexBuffer  = nullptr;

  std::unordered_map<ecs::RenderModel*, ModelBufferCache> m_instanceBufferCache;
  std::vector<DrawData>                              m_drawData;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_BOUNDING_BOX_VISUALIZATION_STRATEGY_H