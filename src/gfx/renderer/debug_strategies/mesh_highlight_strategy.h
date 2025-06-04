#ifndef ARISE_MESH_HIGHLIGHT_STRATEGY_H
#define ARISE_MESH_HIGHLIGHT_STRATEGY_H

#include "gfx/renderer/debug_strategies/debug_draw_strategy.h"

#include <unordered_map>
#include <vector>

namespace arise {
struct RenderModel;
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
 * Renders outlines around selected meshes
 */
class MeshHighlightStrategy : public DebugDrawStrategy {
  public:
  MeshHighlightStrategy() = default;
  ~MeshHighlightStrategy() override { cleanup(); }

  void initialize(rhi::Device*           device,
                  RenderResourceManager* resourceManager,
                  FrameResources*        frameResources,
                  rhi::ShaderManager*    shaderManager) override;

  void resize(const math::Dimension2i& newDimension) override;
  void prepareFrame(const RenderContext& context) override;
  void render(const RenderContext& context) override;
  void cleanup() override;

  bool isExclusive() const override { return false; }

  private:
  struct ModelBufferCache {
    rhi::Buffer* instanceBuffer = nullptr;
    uint32_t     capacity       = 0;
    uint32_t     count          = 0;
  };

  struct HighlightParams {
    math::Vector4f color;
    float           thickness;
    float           xRay;
    float           padding[2];
  };

  struct DrawData {
    rhi::GraphicsPipeline* stencilMarkPipeline          = nullptr;
    rhi::GraphicsPipeline* outlinePipeline              = nullptr;
    rhi::DescriptorSet*    modelMatrixDescriptorSet     = nullptr;
    rhi::DescriptorSet*    highlightParamsDescriptorSet = nullptr;
    rhi::Buffer*           vertexBuffer                 = nullptr;
    rhi::Buffer*           indexBuffer                  = nullptr;
    rhi::Buffer*           instanceBuffer               = nullptr;
    uint32_t               indexCount                   = 0;
    uint32_t               instanceCount                = 0;
  };

  void setupRenderPass_();
  void setupVertexInput_(rhi::GraphicsPipelineDesc& pipelineDesc);
  void createFramebuffers_(const math::Dimension2i& dimension);
  void prepareDrawCalls_(const RenderContext& context);
  void updateInstanceBuffer_(RenderModel*                         model,
                             const std::vector<math::Matrix4f<>>& matrices,
                             ModelBufferCache&                    cache);
  void cleanupUnusedBuffers_(
      const std::unordered_map<RenderModel*, std::vector<math::Matrix4f<>>>& currentFrameInstances);
  rhi::DescriptorSet*    getOrCreateHighlightParamsDescriptorSet_(const math::Vector4f& color,
                                                                  float                  thickness,
                                                                  bool                   xRay);
  rhi::GraphicsPipeline* getOrCreateStencilMarkPipeline_(const std::string& pipelineKey);
  rhi::GraphicsPipeline* getOrCreateOutlinePipeline_(const std::string& pipelineKey, bool xRay);

  const std::string m_stencilMarkVertexShaderPath_ = "assets/shaders/debug/mesh_highlight/stencil_mark.vs.hlsl";
  const std::string m_outlineVertexShaderPath_     = "assets/shaders/debug/mesh_highlight/shader_instancing.vs.hlsl";
  const std::string m_pixelShaderPath_             = "assets/shaders/debug/mesh_highlight/shader.ps.hlsl";

  rhi::Device*           m_device          = nullptr;
  RenderResourceManager* m_resourceManager = nullptr;
  FrameResources*        m_frameResources  = nullptr;
  rhi::ShaderManager*    m_shaderManager   = nullptr;

  rhi::Shader* m_stencilMarkVertexShader = nullptr;
  rhi::Shader* m_outlineVertexShader     = nullptr;
  rhi::Shader* m_pixelShader             = nullptr;

  rhi::Viewport    m_viewport;
  rhi::ScissorRect m_scissor;

  rhi::RenderPass*               m_renderPass = nullptr;
  std::vector<rhi::Framebuffer*> m_framebuffers;

  std::unordered_map<RenderModel*, ModelBufferCache> m_instanceBufferCache;
  std::vector<DrawData>                              m_drawData;

  rhi::DescriptorSetLayout*                                                  m_highlightParamsLayout = nullptr;
  std::unordered_map<uint64_t, std::pair<rhi::Buffer*, rhi::DescriptorSet*>> m_highlightParamsCache;

  struct PipelineCache {
    rhi::GraphicsPipeline* stencilMarkPipeline = nullptr;
    rhi::GraphicsPipeline* outlinePipeline     = nullptr;
  };

  std::unordered_map<std::string, PipelineCache> m_pipelineCache;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_MESH_HIGHLIGHT_STRATEGY_H