#ifndef ARISE_LIGHT_SYSTEM_H
#define ARISE_LIGHT_SYSTEM_H

#include "ecs/components/light.h"
#include "ecs/components/transform.h"
#include "ecs/systems/i_updatable_system.h"
#include "gfx/rhi/interface/buffer.h"
#include "gfx/rhi/interface/descriptor.h"

#include <unordered_set>
#include <vector>

namespace arise::gfx {
namespace rhi {
class Device;
}  // namespace rhi
namespace renderer {
class RenderResourceManager;
}  // namespace renderer
}  // namespace arise::gfx

namespace arise {

class LightSystem : public IUpdatableSystem {
  public:
  LightSystem(gfx::rhi::Device* device, gfx::renderer::RenderResourceManager* resourceManager);
  ~LightSystem();

  void initialize();
  void update(Scene* scene, float deltaTime) override;

  gfx::rhi::DescriptorSet*       getLightDescriptorSet() const { return m_lightDescriptorSet; }
  gfx::rhi::DescriptorSetLayout* getLightDescriptorSetLayout() const { return m_lightLayout; }

  uint32_t getDirectionalLightCount() const { return static_cast<uint32_t>(m_dirLightData.size()); }
  uint32_t getPointLightCount() const { return static_cast<uint32_t>(m_pointLightData.size()); }
  uint32_t getSpotLightCount() const { return static_cast<uint32_t>(m_spotLightData.size()); }

  private:
  struct DirectionalLightData {
    math::Vector3f color;
    float          intensity;
    math::Vector3f direction;
    float          padding;
  };

  struct PointLightData {
    math::Vector3f color;
    float          intensity;
    float          range;
    math::Vector3f position;
  };

  struct SpotLightData {
    math::Vector3f color;
    float          intensity;
    float          range;
    float          innerConeAngle;
    float          outerConeAngle;
    float          padding1;
    math::Vector3f position;
    float          padding2;
    math::Vector3f direction;
    float          padding3;
  };

  struct LightCounts {
    uint32_t directionalLightCount;
    uint32_t pointLightCount;
    uint32_t spotLightCount;
    uint32_t padding;
  };

  void collectDirectionalLights_(Scene* scene);
  void collectPointLights_(Scene* scene);
  void collectSpotLights_(Scene* scene);

  void updateLightBuffers_();
  void createOrUpdateDescriptorSet_();

  void createDescriptorSetLayout_();
  void createEmptyBuffers_();
  void createOrResizeBuffer_(size_t             requiredSize,
                             gfx::rhi::Buffer*& buffer,
                             const std::string& debugName,
                             uint32_t&          currentCapacity,
                             uint32_t           stride);

  void ensureAllBuffersExist_();

  gfx::rhi::Device*                     m_device;
  gfx::renderer::RenderResourceManager* m_resourceManager;

  std::vector<DirectionalLightData> m_dirLightData;
  std::vector<PointLightData>       m_pointLightData;
  std::vector<SpotLightData>        m_spotLightData;

  std::unordered_set<entt::entity> m_prevDirLightEntities;
  std::unordered_set<entt::entity> m_prevPointLightEntities;
  std::unordered_set<entt::entity> m_prevSpotLightEntities;

  gfx::rhi::Buffer* m_dirLightBuffer   = nullptr;
  gfx::rhi::Buffer* m_pointLightBuffer = nullptr;
  gfx::rhi::Buffer* m_spotLightBuffer  = nullptr;
  gfx::rhi::Buffer* m_lightCountBuffer = nullptr;

  gfx::rhi::DescriptorSetLayout* m_lightLayout        = nullptr;
  gfx::rhi::DescriptorSet*       m_lightDescriptorSet = nullptr;

  uint32_t m_dirLightCapacity   = 0;
  uint32_t m_pointLightCapacity = 0;
  uint32_t m_spotLightCapacity  = 0;

  bool m_dirLightsChanged   = true;
  bool m_pointLightsChanged = true;
  bool m_spotLightsChanged  = true;
  bool m_lightCountsChanged = true;
  bool m_initialized        = false;
};

}  // namespace arise

#endif  // ARISE_LIGHT_SYSTEM_H