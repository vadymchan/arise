#include "light_system.h"

#include "gfx/renderer/render_resource_manager.h"
#include "gfx/rhi/interface/device.h"
#include "utils/logger/log.h"
#include "utils/memory/align.h"

namespace arise {
namespace ecs {

LightSystem::LightSystem(gfx::rhi::Device* device, gfx::renderer::RenderResourceManager* resourceManager)
    : m_device(device)
    , m_resourceManager(resourceManager) {
  initialize();
}

LightSystem::~LightSystem() {
}

void LightSystem::initialize() {
  if (m_initialized) {
    return;
  }

  createDescriptorSetLayout_();
  createEmptyBuffers_();
  m_initialized = true;

  LOG_INFO("LightSystem initialized");
}

void LightSystem::update(Scene* scene, float deltaTime) {
  if (!m_initialized) {
    initialize();
  }

  collectDirectionalLights_(scene);
  collectPointLights_(scene);
  collectSpotLights_(scene);

  updateLightBuffers_();

  createOrUpdateDescriptorSet_();
}

void LightSystem::collectDirectionalLights_(Scene* scene) {
  auto& registry = scene->getEntityRegistry();
  auto  view     = registry.view<Light, DirectionalLight>();

  m_dirLightData.clear();

  bool                             anyLightChanged = false;
  std::unordered_set<entt::entity> currentEntities;

  for (auto entity : view) {
    auto& light = view.get<Light>(entity);

    if (!light.enabled) {
      continue;
    }

    auto& dirLight = view.get<DirectionalLight>(entity);

    bool componentChanged = light.isDirty || dirLight.isDirty;

    if (componentChanged) {
      anyLightChanged = true;
    }

    DirectionalLightData data;
    data.color     = light.color;
    data.intensity = light.intensity;
    data.direction = dirLight.direction;
    data.padding   = 0.0f;

    m_dirLightData.push_back(data);
    currentEntities.insert(entity);

    light.isDirty    = false;
    dirLight.isDirty = false;
  }

  bool setChanged        = (m_prevDirLightEntities != currentEntities);
  m_prevDirLightEntities = std::move(currentEntities);

  m_dirLightsChanged = anyLightChanged || setChanged;

  m_lightCountsChanged = m_lightCountsChanged || m_dirLightsChanged;
}

void LightSystem::collectPointLights_(Scene* scene) {
  auto& registry = scene->getEntityRegistry();
  auto  view     = registry.view<Light, PointLight, Transform>();

  m_pointLightData.clear();

  bool                             anyLightChanged = false;
  std::unordered_set<entt::entity> currentEntities;

  for (auto entity : view) {
    auto& light = view.get<Light>(entity);

    if (!light.enabled) {
      continue;
    }

    auto& pointLight = view.get<PointLight>(entity);
    auto& transform  = view.get<Transform>(entity);

    bool componentChanged = light.isDirty || pointLight.isDirty || transform.isDirty;

    if (componentChanged) {
      anyLightChanged = true;
    }

    PointLightData data;
    data.color     = light.color;
    data.intensity = light.intensity;
    data.range     = pointLight.range;
    data.position  = transform.translation;

    m_pointLightData.push_back(data);
    currentEntities.insert(entity);

    light.isDirty      = false;
    pointLight.isDirty = false;
  }

  bool setChanged          = (m_prevPointLightEntities != currentEntities);
  m_prevPointLightEntities = std::move(currentEntities);

  m_pointLightsChanged = anyLightChanged || setChanged;

  m_lightCountsChanged = m_lightCountsChanged || m_pointLightsChanged;
}

void LightSystem::collectSpotLights_(Scene* scene) {
  auto& registry = scene->getEntityRegistry();
  auto  view     = registry.view<Light, SpotLight, Transform>();

  m_spotLightData.clear();

  bool                             anyLightChanged = false;
  std::unordered_set<entt::entity> currentEntities;

  for (auto entity : view) {
    auto& light = view.get<Light>(entity);

    if (!light.enabled) {
      continue;
    }

    auto& spotLight = view.get<SpotLight>(entity);
    auto& transform = view.get<Transform>(entity);

    bool componentChanged = light.isDirty || spotLight.isDirty || transform.isDirty;

    if (componentChanged) {
      anyLightChanged = true;
    }

    SpotLightData data;
    data.color          = light.color;
    data.intensity      = light.intensity;
    data.range          = spotLight.range;
    data.innerConeAngle = spotLight.innerConeAngle;
    data.outerConeAngle = spotLight.outerConeAngle;
    data.position       = transform.translation;

    math::Quaternionf rotation = math::Quaternionf::fromEulerAngles(math::g_degreeToRadian(transform.rotation.x()),
                                                                    math::g_degreeToRadian(transform.rotation.y()),
                                                                    math::g_degreeToRadian(transform.rotation.z()),
                                                                    math::EulerRotationOrder::XYZ);

    math::Vector3f forwardVec(0.0f, 0.0f, 1.0f);
    data.direction = rotation.rotateVector(forwardVec);

    data.padding1 = 0.0f;
    data.padding2 = 0.0f;
    data.padding3 = 0.0f;

    m_spotLightData.push_back(data);
    currentEntities.insert(entity);

    light.isDirty     = false;
    spotLight.isDirty = false;
  }

  bool setChanged         = (m_prevSpotLightEntities != currentEntities);
  m_prevSpotLightEntities = std::move(currentEntities);

  m_spotLightsChanged = anyLightChanged || setChanged;

  m_lightCountsChanged = m_lightCountsChanged || m_spotLightsChanged;
}

void LightSystem::updateLightBuffers_() {
  if (m_dirLightsChanged && !m_dirLightData.empty()) {
    size_t dataSize = m_dirLightData.size() * sizeof(DirectionalLightData);

    if (!m_dirLightBuffer || m_dirLightData.size() > m_dirLightCapacity) {
      createOrResizeBuffer_(
          dataSize, m_dirLightBuffer, "directional_light_buffer", m_dirLightCapacity, sizeof(DirectionalLightData));
    }

    m_device->updateBuffer(m_dirLightBuffer, m_dirLightData.data(), dataSize);
    m_dirLightsChanged = false;
  }

  if (m_pointLightsChanged && !m_pointLightData.empty()) {
    size_t dataSize = m_pointLightData.size() * sizeof(PointLightData);

    if (!m_pointLightBuffer || m_pointLightData.size() > m_pointLightCapacity) {
      createOrResizeBuffer_(
          dataSize, m_pointLightBuffer, "point_light_buffer", m_pointLightCapacity, sizeof(PointLightData));
    }

    m_device->updateBuffer(m_pointLightBuffer, m_pointLightData.data(), dataSize);
    m_pointLightsChanged = false;
  }

  if (m_spotLightsChanged && !m_spotLightData.empty()) {
    size_t dataSize = m_spotLightData.size() * sizeof(SpotLightData);

    if (!m_spotLightBuffer || m_spotLightData.size() > m_spotLightCapacity) {
      createOrResizeBuffer_(
          dataSize, m_spotLightBuffer, "spot_light_buffer", m_spotLightCapacity, sizeof(SpotLightData));
    }

    m_device->updateBuffer(m_spotLightBuffer, m_spotLightData.data(), dataSize);
    m_spotLightsChanged = false;
  }

  if (m_lightCountsChanged) {
    LightCounts counts;
    counts.directionalLightCount = static_cast<uint32_t>(m_dirLightData.size());
    counts.pointLightCount       = static_cast<uint32_t>(m_pointLightData.size());
    counts.spotLightCount        = static_cast<uint32_t>(m_spotLightData.size());
    counts.padding               = 0;

    if (!m_lightCountBuffer) {
      gfx::rhi::BufferDesc countDesc;
      countDesc.size        = alignConstantBufferSize(sizeof(LightCounts));
      countDesc.type        = gfx::rhi::BufferType::Dynamic;
      countDesc.createFlags = gfx::rhi::BufferCreateFlag::CpuAccess | gfx::rhi::BufferCreateFlag::ConstantBuffer;
      countDesc.debugName   = "light_count_buffer";

      auto buffer        = m_device->createBuffer(countDesc);
      m_lightCountBuffer = m_resourceManager->addBuffer(std::move(buffer), "light_count_buffer");
    }

    m_device->updateBuffer(m_lightCountBuffer, &counts, sizeof(counts));
    m_lightCountsChanged = false;
  }
}

void LightSystem::createOrUpdateDescriptorSet_() {
  ensureAllBuffersExist_();

  if (!m_lightDescriptorSet) {
    auto descriptorSet   = m_device->createDescriptorSet(m_lightLayout);
    m_lightDescriptorSet = m_resourceManager->addDescriptorSet(std::move(descriptorSet), "light_descriptor");
    LOG_INFO("Created new light descriptor set");
  }

  if (m_lightCountBuffer) {
    m_lightDescriptorSet->setUniformBuffer(0, m_lightCountBuffer);
  }

  m_lightDescriptorSet->setStorageBuffer(1, m_dirLightBuffer);
  m_lightDescriptorSet->setStorageBuffer(2, m_pointLightBuffer);
  m_lightDescriptorSet->setStorageBuffer(3, m_spotLightBuffer);
}

void LightSystem::createDescriptorSetLayout_() {
  gfx::rhi::DescriptorSetLayoutDesc layoutDesc;

  gfx::rhi::DescriptorSetLayoutBindingDesc countBinding;
  countBinding.binding         = 0;
  countBinding.type            = gfx::rhi::ShaderBindingType::Uniformbuffer;
  countBinding.stageFlags      = gfx::rhi::ShaderStageFlag::Fragment;
  countBinding.descriptorCount = 1;
  layoutDesc.bindings.push_back(countBinding);

  // Directional lights
  gfx::rhi::DescriptorSetLayoutBindingDesc dirBinding;
  dirBinding.binding         = 1;  // t0
  dirBinding.type            = gfx::rhi::ShaderBindingType::BufferSrv;
  dirBinding.stageFlags      = gfx::rhi::ShaderStageFlag::Fragment;
  dirBinding.descriptorCount = 1;
  layoutDesc.bindings.push_back(dirBinding);

  // Point lights
  gfx::rhi::DescriptorSetLayoutBindingDesc pointBinding;
  pointBinding.binding         = 2;  // t1
  pointBinding.type            = gfx::rhi::ShaderBindingType::BufferSrv;
  pointBinding.stageFlags      = gfx::rhi::ShaderStageFlag::Fragment;
  pointBinding.descriptorCount = 1;
  layoutDesc.bindings.push_back(pointBinding);

  // Spot lights
  gfx::rhi::DescriptorSetLayoutBindingDesc spotBinding;
  spotBinding.binding         = 3;  // t2
  spotBinding.type            = gfx::rhi::ShaderBindingType::BufferSrv;
  spotBinding.stageFlags      = gfx::rhi::ShaderStageFlag::Fragment;
  spotBinding.descriptorCount = 1;
  layoutDesc.bindings.push_back(spotBinding);

  auto layout   = m_device->createDescriptorSetLayout(layoutDesc);
  m_lightLayout = m_resourceManager->addDescriptorSetLayout(std::move(layout), "light_descriptor_layout");
}

void LightSystem::createEmptyBuffers_() {
  LightCounts counts           = {};
  counts.directionalLightCount = 0;
  counts.pointLightCount       = 0;
  counts.spotLightCount        = 0;
  counts.padding               = 0;

  gfx::rhi::BufferDesc countDesc;
  countDesc.size        = alignConstantBufferSize(sizeof(LightCounts));
  countDesc.type        = gfx::rhi::BufferType::Dynamic;
  countDesc.createFlags = gfx::rhi::BufferCreateFlag::CpuAccess | gfx::rhi::BufferCreateFlag::ConstantBuffer;
  countDesc.debugName   = "light_count_buffer";
  auto buffer           = m_device->createBuffer(countDesc);
  m_lightCountBuffer    = m_resourceManager->addBuffer(std::move(buffer), "light_count_buffer");
  m_device->updateBuffer(m_lightCountBuffer, &counts, sizeof(counts));

  gfx::rhi::BufferDesc dirDesc;
  dirDesc.size        = alignConstantBufferSize(sizeof(DirectionalLightData));
  dirDesc.createFlags = gfx::rhi::BufferCreateFlag::CpuAccess | gfx::rhi::BufferCreateFlag::ShaderResource;
  dirDesc.type        = gfx::rhi::BufferType::Dynamic;
  dirDesc.stride      = sizeof(DirectionalLightData);
  dirDesc.debugName   = "empty_directional_light_buffer";
  auto dirBuffer      = m_device->createBuffer(dirDesc);
  m_dirLightBuffer    = m_resourceManager->addBuffer(std::move(dirBuffer), "empty_directional_light_buffer");

  gfx::rhi::BufferDesc pointDesc;
  pointDesc.size        = alignConstantBufferSize(sizeof(PointLightData));
  pointDesc.createFlags = gfx::rhi::BufferCreateFlag::CpuAccess | gfx::rhi::BufferCreateFlag::ShaderResource;
  pointDesc.type        = gfx::rhi::BufferType::Dynamic;
  pointDesc.stride      = sizeof(PointLightData);
  pointDesc.debugName   = "empty_point_light_buffer";
  auto pointBuffer      = m_device->createBuffer(pointDesc);
  m_pointLightBuffer    = m_resourceManager->addBuffer(std::move(pointBuffer), "empty_point_light_buffer");

  gfx::rhi::BufferDesc spotDesc;
  spotDesc.size        = alignConstantBufferSize(sizeof(SpotLightData));
  spotDesc.createFlags = gfx::rhi::BufferCreateFlag::CpuAccess | gfx::rhi::BufferCreateFlag::ShaderResource;
  spotDesc.type        = gfx::rhi::BufferType::Dynamic;
  spotDesc.stride      = sizeof(SpotLightData);
  spotDesc.debugName   = "empty_spot_light_buffer";
  auto spotBuffer      = m_device->createBuffer(spotDesc);
  m_spotLightBuffer    = m_resourceManager->addBuffer(std::move(spotBuffer), "empty_spot_light_buffer");

  m_dirLightCapacity   = 1;
  m_pointLightCapacity = 1;
  m_spotLightCapacity  = 1;

  LOG_INFO("Created empty light buffers for Vulkan compatibility");
}

void LightSystem::createOrResizeBuffer_(size_t             requiredSize,
                                        gfx::rhi::Buffer*& buffer,
                                        const std::string& debugName,
                                        uint32_t&          currentCapacity,
                                        uint32_t           stride) {
  if (!buffer || requiredSize > currentCapacity * stride) {
    uint32_t newCapacity = std::max(static_cast<uint32_t>(requiredSize / stride * 1.5), 8u);
    uint32_t newSize     = newCapacity * stride;

    gfx::rhi::BufferDesc bufferDesc;
    bufferDesc.size        = alignConstantBufferSize(newSize);
    bufferDesc.createFlags = gfx::rhi::BufferCreateFlag::CpuAccess | gfx::rhi::BufferCreateFlag::ShaderResource;
    bufferDesc.type        = gfx::rhi::BufferType::Dynamic;
    bufferDesc.stride      = stride;
    bufferDesc.debugName   = debugName;

    auto newBuffer = m_device->createBuffer(bufferDesc);

    buffer          = m_resourceManager->addBuffer(std::move(newBuffer), debugName);
    currentCapacity = newCapacity;

    LOG_INFO("Created/Resized light buffer: {} with capacity: {}", debugName, newCapacity);
  }
}

void LightSystem::ensureAllBuffersExist_() {
  if (!m_lightCountBuffer) {
    createEmptyBuffers_();
    return;
  }

  if (!m_dirLightBuffer) {
    gfx::rhi::BufferDesc dirDesc;
    dirDesc.size        = alignConstantBufferSize(sizeof(DirectionalLightData));
    dirDesc.createFlags = gfx::rhi::BufferCreateFlag::CpuAccess | gfx::rhi::BufferCreateFlag::ShaderResource;
    dirDesc.type        = gfx::rhi::BufferType::Dynamic;
    dirDesc.stride      = sizeof(DirectionalLightData);
    dirDesc.debugName   = "empty_directional_light_buffer";
    auto dirBuffer      = m_device->createBuffer(dirDesc);
    m_dirLightBuffer    = m_resourceManager->addBuffer(std::move(dirBuffer), "empty_directional_light_buffer");
    m_dirLightCapacity  = 1;
  }

  if (!m_pointLightBuffer) {
    gfx::rhi::BufferDesc pointDesc;
    pointDesc.size        = alignConstantBufferSize(sizeof(PointLightData));
    pointDesc.createFlags = gfx::rhi::BufferCreateFlag::CpuAccess | gfx::rhi::BufferCreateFlag::ShaderResource;
    pointDesc.type        = gfx::rhi::BufferType::Dynamic;
    pointDesc.stride      = sizeof(PointLightData);
    pointDesc.debugName   = "empty_point_light_buffer";
    auto pointBuffer      = m_device->createBuffer(pointDesc);
    m_pointLightBuffer    = m_resourceManager->addBuffer(std::move(pointBuffer), "empty_point_light_buffer");
    m_pointLightCapacity  = 1;
  }

  if (!m_spotLightBuffer) {
    gfx::rhi::BufferDesc spotDesc;
    spotDesc.size        = alignConstantBufferSize(sizeof(SpotLightData));
    spotDesc.createFlags = gfx::rhi::BufferCreateFlag::CpuAccess | gfx::rhi::BufferCreateFlag::ShaderResource;
    spotDesc.type        = gfx::rhi::BufferType::Dynamic;
    spotDesc.stride      = sizeof(SpotLightData);
    spotDesc.debugName   = "empty_spot_light_buffer";
    auto spotBuffer      = m_device->createBuffer(spotDesc);
    m_spotLightBuffer    = m_resourceManager->addBuffer(std::move(spotBuffer), "empty_spot_light_buffer");
    m_spotLightCapacity  = 1;
  }
}
}  // namespace ecs
}  // namespace arise