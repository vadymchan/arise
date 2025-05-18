#ifndef GAME_ENGINE_BASE_PASS_H
#define GAME_ENGINE_BASE_PASS_H

#include "gfx/renderer/render_pass.h"
#include "gfx/rhi/interface/render_pass.h"

#include <unordered_map>
#include <vector>

namespace game_engine {
struct RenderModel;
struct Material;
}  // namespace game_engine

namespace game_engine::gfx::rhi {
class Buffer;
class DescriptorSet;
class GraphicsPipeline;
}  // namespace game_engine::gfx::rhi

namespace game_engine {
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

  void resize(const math::Dimension2Di& newDimension) override;

  void prepareFrame(const RenderContext& context) override;

  void render(const RenderContext& context) override;

  void endFrame() override { m_drawData.clear(); }

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

  void createFramebuffer_(const math::Dimension2Di& dimension);

  void updateInstanceBuffer_(RenderModel*                         model,
                             const std::vector<math::Matrix4f<>>& matrices,
                             ModelBufferCache&                    cache);

  void prepareDrawCalls_(const RenderContext& context);

  void cleanupUnusedBuffers_(
      const std::unordered_map<RenderModel*, std::vector<math::Matrix4f<>>>& currentFrameInstances);

  const std::string m_vertexShaderPath_ = "assets/shaders/base_pass/shader_instancing.vs.hlsl";
  const std::string m_pixelShaderPath_  = "assets/shaders/base_pass/shader.ps.hlsl";

  rhi::DescriptorSet* getOrCreateMaterialDescriptorSet_(Material* material);

  rhi::Device*           m_device          = nullptr;
  RenderResourceManager* m_resourceManager = nullptr;
  FrameResources*        m_frameResources  = nullptr;

  rhi::RenderPass*               m_renderPass = nullptr;
  std::vector<rhi::Framebuffer*> m_framebuffers;
  rhi::Shader*                   m_vertexShader = nullptr;
  rhi::Shader*                   m_pixelShader  = nullptr;

  rhi::Viewport    m_viewport;
  rhi::ScissorRect m_scissor;

  std::unordered_map<RenderModel*, ModelBufferCache> m_instanceBufferCache;
  std::vector<DrawData>                              m_drawData;

  struct MaterialCache {
    rhi::DescriptorSet* descriptorSet = nullptr;
  };

  std::unordered_map<Material*, MaterialCache> m_materialCache;

  rhi::ShaderManager* m_shaderManager = nullptr;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace game_engine

#endif  // GAME_ENGINE_BASE_PASS_H