#ifndef ARISE_FRAME_RESOURCES_H
#define ARISE_FRAME_RESOURCES_H

#include "ecs/components/render_model.h"
#include "ecs/components/transform.h"
#include "gfx/renderer/render_context.h"
#include "gfx/rhi/interface/buffer.h"
#include "gfx/rhi/interface/descriptor.h"
#include "gfx/rhi/interface/device.h"
#include "gfx/rhi/interface/sampler.h"
#include "gfx/rhi/interface/texture.h"
#include "utils/math/math_util.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace arise {
namespace ecs {
class LightSystem;
}  // namespace ecs
}  // namespace arise

namespace arise {
namespace gfx {
namespace renderer {

class RenderResourceManager;

/**
 * Manages shared resources used across different render passes
 */
class FrameResources {
  public:
  FrameResources(rhi::Device* device, RenderResourceManager* resourceManager);

  ~FrameResources() { cleanup(); }

  void initialize(uint32_t framesCount);

  /**
   * Resize resources when viewport (window / editor viewport) changes
   */
  void resize(const math::Dimension2i& newDimension);

  void updatePerFrameResources(const RenderContext& context);

  void clearSceneResources();
  void cleanup();

  struct RenderTargets {
    std::unique_ptr<rhi::Texture> colorBuffer;
    std::unique_ptr<rhi::Texture> depthBuffer;
    rhi::Texture*                 backBuffer = nullptr;
  };

  RenderTargets& getRenderTargets(uint32_t frameIndex) {
    return m_renderTargetsPerFrame[frameIndex % m_renderTargetsPerFrame.size()];
  }

  uint32_t getFramesCount() const { return m_renderTargetsPerFrame.size(); }

  const rhi::Viewport&    getViewport() const { return m_viewport; }
  const rhi::ScissorRect& getScissor() const { return m_scissor; }

  rhi::DescriptorSet* getViewDescriptorSet() const { return m_viewDescriptorSet; }
  rhi::DescriptorSet* getDefaultSamplerDescriptorSet() const { return m_defaultSamplerDescriptorSet; }
  rhi::DescriptorSet* getLightDescriptorSet() const;
  rhi::DescriptorSet* getOrCreateModelMatrixDescriptorSet(ecs::RenderMesh* renderMesh);

  rhi::Sampler* getDefaultSampler() const { return m_defaultSampler; }

  struct ModelInstance {
    ecs::RenderModel* model;
    ecs::Transform    transform;
    math::Matrix4f<>  modelMatrix;
    entt::entity      entityId;

    uint32_t materialId = 0;  // for sorting

    bool isDirty = false;
  };

  /**
   * Returns models sorted by material ID for optimal rendering batching.
   * This helps reduce state changes during rendering.
   */
  const std::vector<ModelInstance*>& getModels() const { return m_sortedModels; }

  rhi::DescriptorSetLayout* getViewDescriptorSetLayout() const { return m_viewDescriptorSetLayout; }
  rhi::DescriptorSetLayout* getModelMatrixDescriptorSetLayout() const { return m_modelMatrixDescriptorSetLayout; }
  rhi::DescriptorSetLayout* getLightDescriptorSetLayout() const;
  rhi::DescriptorSetLayout* getMaterialDescriptorSetLayout() const { return m_materialDescriptorSetLayout; }

  rhi::Buffer* getOrCreateMaterialParamBuffer(ecs::Material* material);

  rhi::Texture* getDefaultWhiteTexture() const { return m_defaultWhiteTexture; }
  rhi::Texture* getDefaultNormalTexture() const { return m_defaultNormalTexture; }
  rhi::Texture* getDefaultBlackTexture() const { return m_defaultBlackTexture; }

  private:
  void createViewDescriptorSetLayout_();
  void createModelMatrixDescriptorSetLayout_();
  void createMaterialDescriptorSetLayout_();
  void createDefaultTextures_();
  void createDefaultSampler_();
  void createSamplerDescriptorSet_();
  void createRenderTargets_(RenderTargets& targets, const math::Dimension2i& dimensions);

  void updateViewResources_(const RenderContext& context);
  void updateModelList_(const RenderContext& context);

  void sortModelsByMaterial_();

  void clearInternalDirtyFlags_();
  void clearEntityDirtyFlags_(const RenderContext& context);

  rhi::Device*           m_device          = nullptr;
  RenderResourceManager* m_resourceManager = nullptr;
  FrameResources*        m_frameResources  = nullptr;

  bool m_initialized = false;

  rhi::Viewport    m_viewport;
  rhi::ScissorRect m_scissor;

  std::vector<RenderTargets> m_renderTargetsPerFrame;

  rhi::DescriptorSet* m_viewDescriptorSet           = nullptr;
  rhi::DescriptorSet* m_defaultSamplerDescriptorSet = nullptr;

  rhi::DescriptorSetLayout* m_viewDescriptorSetLayout        = nullptr;
  rhi::DescriptorSetLayout* m_modelMatrixDescriptorSetLayout = nullptr;
  rhi::DescriptorSetLayout* m_materialDescriptorSetLayout    = nullptr;

  rhi::Buffer* m_viewUniformBuffer = nullptr;

  rhi::Texture* m_defaultWhiteTexture  = nullptr;
  rhi::Texture* m_defaultNormalTexture = nullptr;
  rhi::Texture* m_defaultBlackTexture  = nullptr;

  rhi::Sampler* m_defaultSampler = nullptr;

  struct ModelMatrixCache {
    rhi::DescriptorSet* descriptorSet = nullptr;
  };

  std::unordered_map<ecs::RenderMesh*, ModelMatrixCache> m_modelMatrixCache;

  struct MaterialParametersData {
    math::Vector4f baseColor;
    float          metallic;
    float          roughness;
    float          opacity;
    float          padding;
  };

  struct MaterialParamCache {
    rhi::Buffer* paramBuffer = nullptr;
  };

  std::unordered_map<ecs::Material*, MaterialParamCache> m_materialParamCache;

  std::unordered_map<entt::entity, ModelInstance> m_modelsMap;
  std::vector<ModelInstance*>                     m_sortedModels;

  ecs::LightSystem* m_lightSystem = nullptr;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_FRAME_RESOURCES_H