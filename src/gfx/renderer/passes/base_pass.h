#ifndef ARISE_BASE_PASS_H
#define ARISE_BASE_PASS_H

#include "gfx/renderer/render_pass.h"
#include "gfx/rhi/interface/render_pass.h"
#include "gfx/rhi/shader_reflection/pipeline_layout_manager.h"

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
class GraphicsPipeline;
}  // namespace arise::gfx::rhi

namespace arise {
namespace gfx {
namespace renderer {

/**
 * Handles the main rendering of scene objects
 */
class BasePass : public RenderPass {
  public:
  BasePass() = default;

  ~BasePass() override { cleanup(); }

  void initialize(rhi::Device*           device,
                  RenderResourceManager* resourceManager,
                  FrameResources*        frameResources,
                  rhi::ShaderManager*    shaderManager) override;

  void resize(const math::Dimension2i& newDimension) override;

  void prepareFrame(const RenderContext& context) override;

  void render(const RenderContext& context) override;

  void endFrame() override { m_drawData.clear(); }

  void clearSceneResources();
  void cleanup() override;

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

  void createFramebuffer_(const math::Dimension2i& dimension);

  void updateInstanceBuffer_(ecs::RenderModel*                    model,
                             const std::vector<math::Matrix4f<>>& matrices,
                             ModelBufferCache&                    cache);

  void prepareDrawCalls_(const RenderContext& context);

  void cleanupUnusedBuffers_(
      const std::unordered_map<ecs::RenderModel*, std::vector<math::Matrix4f<>>>& currentFrameInstances);

  const std::string m_vertexShaderPath_ = "assets/shaders/base_pass/shader_instancing.vs.hlsl";
  const std::string m_pixelShaderPath_  = "assets/shaders/base_pass/shader.ps.hlsl";

  rhi::DescriptorSet* getOrCreateMaterialDescriptorSet_(ecs::Material* material);

  rhi::Device*           m_device          = nullptr;
  RenderResourceManager* m_resourceManager = nullptr;
  FrameResources*        m_frameResources  = nullptr;

  rhi::RenderPass*               m_renderPass = nullptr;
  std::vector<rhi::Framebuffer*> m_framebuffers;
  rhi::Shader*                   m_vertexShader = nullptr;
  rhi::Shader*                   m_pixelShader  = nullptr;

  rhi::Viewport    m_viewport;
  rhi::ScissorRect m_scissor;

  std::unordered_map<ecs::RenderModel*, ModelBufferCache> m_instanceBufferCache;
  std::vector<DrawData>                                   m_drawData;

  struct MaterialCache {
    rhi::DescriptorSet* descriptorSet = nullptr;
  };

  std::unordered_map<ecs::Material*, MaterialCache> m_materialCache;

  rhi::ShaderManager* m_shaderManager = nullptr;

  rhi::PipelineLayoutManager m_layoutManager;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_BASE_PASS_H