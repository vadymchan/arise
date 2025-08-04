#ifndef ARISE_NORMAL_MAP_VISUALIZATION_STRATEGY_H
#define ARISE_NORMAL_MAP_VISUALIZATION_STRATEGY_H

#include "gfx/renderer/debug_strategies/debug_draw_strategy.h"

#include <unordered_map>
#include <vector>

namespace arise {
namespace ecs {
struct RenderModel;
struct Material;
}  // namespace ecs
}  // namespace arise

namespace arise::gfx::rhi {
class Buffer;
class DescriptorSet;
class DescriptorSetLayout;
class GraphicsPipeline;
class RenderPass;
}  // namespace arise::gfx::rhi

namespace arise {
namespace gfx {
namespace renderer {

class NormalMapVisualizationStrategy : public DebugDrawStrategy {
  public:
  NormalMapVisualizationStrategy() = default;

  ~NormalMapVisualizationStrategy() override { cleanup(); }

  void initialize(rhi::Device*           device,
                  RenderResourceManager* resourceManager,
                  FrameResources*        frameResources,
                  rhi::ShaderManager*    shaderManager) override;

  void resize(const math::Dimension2i& newDimension) override;
  void prepareFrame(const RenderContext& context) override;
  void render(const RenderContext& context) override;
  void clearSceneResources() override;
  void cleanup() override;

  bool isExclusive() const override { return true; }

  private:
  struct ModelBufferCache {
    rhi::Buffer* instanceBuffer = nullptr;
    uint32_t     capacity       = 0;
    uint32_t     count          = 0;
  };

  struct DrawData {
    rhi::GraphicsPipeline* pipeline                 = nullptr;
    rhi::DescriptorSet*    modelMatrixDescriptorSet = nullptr;
    rhi::DescriptorSet*    materialDescriptorSet    = nullptr;
    rhi::Buffer*           vertexBuffer             = nullptr;
    rhi::Buffer*           indexBuffer              = nullptr;
    rhi::Buffer*           instanceBuffer           = nullptr;
    uint32_t               indexCount               = 0;
    uint32_t               instanceCount            = 0;
  };

  void setupRenderPass_();
  void createFramebuffers_(const math::Dimension2i& dimension);
  void prepareDrawCalls_(const RenderContext& context);
  void updateInstanceBuffer_(ecs::RenderModel*                    model,
                             const std::vector<math::Matrix4f<>>& matrices,
                             ModelBufferCache&                    cache);
  void cleanupUnusedBuffers_(
      const std::unordered_map<ecs::RenderModel*, std::vector<math::Matrix4f<>>>& currentFrameInstances);
  rhi::DescriptorSet* getOrCreateMaterialDescriptorSet_(ecs::Material* material);

  const std::string m_vertexShaderPath_ = "assets/shaders/debug/normal_map_visualization/shader_instancing.vs.hlsl";
  const std::string m_pixelShaderPath_  = "assets/shaders/debug/normal_map_visualization/shader.ps.hlsl";

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
  rhi::DescriptorSetLayout*      m_materialDescriptorSetLayout = nullptr;

  struct MaterialCache {
    rhi::DescriptorSet* descriptorSet = nullptr;
  };

  std::unordered_map<ecs::Material*, MaterialCache> m_materialCache;

  std::unordered_map<ecs::RenderModel*, ModelBufferCache> m_instanceBufferCache;
  std::vector<DrawData>                                   m_drawData;
};
}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_NORMAL_MAP_VISUALIZATION_STRATEGY_H