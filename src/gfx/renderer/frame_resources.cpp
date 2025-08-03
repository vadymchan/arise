#include "gfx/renderer/frame_resources.h"

#include "ecs/components/camera.h"
#include "ecs/components/light.h"
#include "ecs/systems/light_system.h"
#include "ecs/systems/system_manager.h"
#include "gfx/renderer/render_resource_manager.h"
#include "utils/memory/align.h"
#include "utils/service/service_locator.h"
#include "profiler/profiler.h"


namespace arise {
namespace gfx {
namespace renderer {

FrameResources::FrameResources(rhi::Device* device, RenderResourceManager* resourceManager)
    : m_device(device)
    , m_resourceManager(resourceManager) {
}

void FrameResources::initialize(uint32_t framesCount) {
  if (m_initialized) {
    return;
  }

  createViewDescriptorSetLayout_();
  createModelMatrixDescriptorSetLayout_();
  createMaterialDescriptorSetLayout_();
  createDefaultTextures_();

  createDefaultSampler_();
  createSamplerDescriptorSet_();

  m_renderTargetsPerFrame.resize(framesCount);

  m_initialized = true;
}

void FrameResources::resize(const math::Dimension2i& newDimension) {
  m_viewport.x        = 0.0f;
  m_viewport.y        = 0.0f;
  m_viewport.width    = static_cast<float>(newDimension.width());
  m_viewport.height   = static_cast<float>(newDimension.height());
  m_viewport.minDepth = 0.0f;
  m_viewport.maxDepth = 1.0f;

  m_scissor.x      = 0;
  m_scissor.y      = 0;
  m_scissor.width  = newDimension.width();
  m_scissor.height = newDimension.height();

  for (auto& target : m_renderTargetsPerFrame) {
    createRenderTargets_(target, newDimension);
  }
}

void FrameResources::updatePerFrameResources(const RenderContext& context) {
  CPU_ZONE_NC("FrameResources::updatePerFrameResources", color::YELLOW);
  if (!m_lightSystem) {
    auto systemManager = ServiceLocator::s_get<ecs::SystemManager>();
    m_lightSystem      = systemManager->getSystem<ecs::LightSystem>();
    if (!m_lightSystem) {
      GlobalLogger::Log(LogLevel::Error, "LightSystem not found!");
      return;
    }
  }

  clearInternalDirtyFlags_();

  updateViewResources_(context);
  updateModelList_(context);

  clearEntityDirtyFlags_(context);
}

void FrameResources::clearSceneResources() {
  m_modelsMap.clear();
  m_sortedModels.clear();
  m_modelMatrixCache.clear();
  m_materialParamCache.clear();
  GlobalLogger::Log(LogLevel::Info, "Frame resources cleared for scene switch");
}

void FrameResources::cleanup() {
  m_renderTargetsPerFrame.clear();

  m_viewDescriptorSet       = nullptr;
  m_viewDescriptorSetLayout = nullptr;
  m_viewUniformBuffer       = nullptr;

  m_defaultSamplerDescriptorSet = nullptr;
  m_defaultSampler              = nullptr;

  m_modelMatrixCache.clear();
  m_materialParamCache.clear();

  m_sortedModels.clear();
  m_modelsMap.clear();

  m_initialized = false;
}

rhi::DescriptorSet* FrameResources::getLightDescriptorSet() const {
  return m_lightSystem->getLightDescriptorSet();
}

rhi::DescriptorSet* FrameResources::getOrCreateModelMatrixDescriptorSet(ecs::RenderMesh* renderMesh) {
  auto it = m_modelMatrixCache.find(renderMesh);
  if (it != m_modelMatrixCache.end() && it->second.descriptorSet) {
    return it->second.descriptorSet;
  }

  if (!renderMesh->transformMatrixBuffer) {
    GlobalLogger::Log(LogLevel::Warning, "RenderMesh has no model matrix buffer");
    return nullptr;
  }

  auto descriptorSet = m_device->createDescriptorSet(m_modelMatrixDescriptorSetLayout);
  descriptorSet->setUniformBuffer(0, renderMesh->transformMatrixBuffer);

  std::string setKey           = "mesh_model_matrix_set_" + std::to_string(reinterpret_cast<uintptr_t>(renderMesh));
  auto        descriptorSetPtr = m_resourceManager->addDescriptorSet(std::move(descriptorSet), setKey);

  m_modelMatrixCache[renderMesh].descriptorSet = descriptorSetPtr;
  return descriptorSetPtr;
}

rhi::DescriptorSetLayout* FrameResources::getLightDescriptorSetLayout() const {
  return m_lightSystem->getLightDescriptorSetLayout();
}

rhi::Buffer* FrameResources::getOrCreateMaterialParamBuffer(ecs::Material* material) {
  if (!material) {
    GlobalLogger::Log(LogLevel::Warning, "Material is null");
    return nullptr;
  }

  auto it = m_materialParamCache.find(material);
  if (it != m_materialParamCache.end() && it->second.paramBuffer) {
    return it->second.paramBuffer;
  }

  std::string bufferKey = "material_params_" + std::to_string(reinterpret_cast<uintptr_t>(material));

  rhi::BufferDesc bufferDesc;
  bufferDesc.size        = alignConstantBufferSize(sizeof(MaterialParametersData));
  bufferDesc.createFlags = rhi::BufferCreateFlag::CpuAccess | rhi::BufferCreateFlag::ConstantBuffer;
  bufferDesc.type        = rhi::BufferType::Dynamic;
  bufferDesc.debugName   = bufferKey;

  auto buffer    = m_device->createBuffer(bufferDesc);
  auto bufferPtr = m_resourceManager->addBuffer(std::move(buffer), bufferKey);

  MaterialParametersData paramData = {};

  auto colorIt = material->vectorParameters.find("base_color");
  if (colorIt != material->vectorParameters.end()) {
    paramData.baseColor = colorIt->second;
  } else {
    paramData.baseColor = math::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
  }

  auto metallicIt = material->scalarParameters.find("metallic");
  if (metallicIt != material->scalarParameters.end()) {
    paramData.metallic = metallicIt->second;
  } else {
    paramData.metallic = 0.0f;
  }

  auto roughnessIt = material->scalarParameters.find("roughness");
  if (roughnessIt != material->scalarParameters.end()) {
    paramData.roughness = roughnessIt->second;
  } else {
    paramData.roughness = 1.0f;
  }

  auto opacityIt = material->scalarParameters.find("opacity");
  if (opacityIt != material->scalarParameters.end()) {
    paramData.opacity = opacityIt->second;
  } else {
    paramData.opacity = 1.0f;
  }

  m_device->updateBuffer(bufferPtr, &paramData, sizeof(paramData));

  m_materialParamCache[material].paramBuffer = bufferPtr;

  return bufferPtr;
}

void FrameResources::createViewDescriptorSetLayout_() {
  rhi::DescriptorSetLayoutDesc        viewLayoutDesc;
  rhi::DescriptorSetLayoutBindingDesc viewBindingDesc;
  viewBindingDesc.binding    = 0;
  viewBindingDesc.type       = rhi::ShaderBindingType::Uniformbuffer;
  viewBindingDesc.stageFlags = rhi::ShaderStageFlag::Vertex | rhi::ShaderStageFlag::Fragment;
  viewLayoutDesc.bindings.push_back(viewBindingDesc);

  auto viewSetLayout        = m_device->createDescriptorSetLayout(viewLayoutDesc);
  m_viewDescriptorSetLayout = m_resourceManager->addDescriptorSetLayout(std::move(viewSetLayout), "view_set_layout");
}

void FrameResources::createModelMatrixDescriptorSetLayout_() {
  rhi::DescriptorSetLayoutDesc        layoutDesc;
  rhi::DescriptorSetLayoutBindingDesc bindingDesc;
  bindingDesc.binding    = 0;
  bindingDesc.type       = rhi::ShaderBindingType::Uniformbuffer;
  bindingDesc.stageFlags = rhi::ShaderStageFlag::Vertex;
  layoutDesc.bindings.push_back(bindingDesc);

  auto layout = m_device->createDescriptorSetLayout(layoutDesc);
  m_modelMatrixDescriptorSetLayout
      = m_resourceManager->addDescriptorSetLayout(std::move(layout), "model_matrix_layout");
}

void FrameResources::createMaterialDescriptorSetLayout_() {
  rhi::DescriptorSetLayoutDesc materialLayoutDesc;

  rhi::DescriptorSetLayoutBindingDesc paramsBindingDesc;
  paramsBindingDesc.binding    = 0;
  paramsBindingDesc.type       = rhi::ShaderBindingType::Uniformbuffer;
  paramsBindingDesc.stageFlags = rhi::ShaderStageFlag::Fragment;
  materialLayoutDesc.bindings.push_back(paramsBindingDesc);

  std::vector<std::string> textureNames = {"albedo", "normal_map", "metallic_roughness"};
  uint32_t                 binding      = 1;

  for (const auto& textureName : textureNames) {
    rhi::DescriptorSetLayoutBindingDesc textureBindingDesc;
    textureBindingDesc.binding    = binding++;
    textureBindingDesc.type       = rhi::ShaderBindingType::TextureSrv;
    textureBindingDesc.stageFlags = rhi::ShaderStageFlag::Fragment;
    materialLayoutDesc.bindings.push_back(textureBindingDesc);
  }

  auto materialSetLayout = m_device->createDescriptorSetLayout(materialLayoutDesc);
  m_materialDescriptorSetLayout
      = m_resourceManager->addDescriptorSetLayout(std::move(materialSetLayout), "material_layout");
}

void FrameResources::createDefaultTextures_() {
  // 1x1 white texture (for albedo when missing)
  {
    rhi::TextureDesc texDesc;
    texDesc.width = texDesc.height = 1;
    texDesc.format                 = rhi::TextureFormat::Rgba8;
    texDesc.createFlags            = gfx::rhi::TextureCreateFlag::TransferDst;
    texDesc.initialLayout          = rhi::ResourceLayout::ShaderReadOnly;
    texDesc.debugName              = "default_white_texture";

    auto texture = m_device->createTexture(texDesc);

    // white color
    uint32_t whitePixel = 0xFF'FF'FF'FF;
    m_device->updateTexture(texture.get(), &whitePixel, sizeof(whitePixel));

    m_defaultWhiteTexture = m_resourceManager->addTexture(std::move(texture), "default_white_texture");
  }

  // 1x1 normal map texture (0.5, 0.5, 1.0 for flat normal)
  {
    rhi::TextureDesc texDesc;
    texDesc.width = texDesc.height = 1;
    texDesc.format                 = rhi::TextureFormat::Rgba8;
    texDesc.createFlags            = gfx::rhi::TextureCreateFlag::TransferDst;
    texDesc.initialLayout          = rhi::ResourceLayout::ShaderReadOnly;
    texDesc.debugName              = "default_normal_texture";

    auto texture = m_device->createTexture(texDesc);

    // normal facing up (0.5, 0.5, 1.0, 1.0) encoded as RGBA
    const uint8_t normalPixel[4] = {0x80, 0x80, 0xFF, 0xFF};
    m_device->updateTexture(texture.get(), &normalPixel, sizeof(normalPixel));

    m_defaultNormalTexture = m_resourceManager->addTexture(std::move(texture), "default_normal_texture");
  }

  // 1x1 black texture (for metallic-roughness where black = non-metallic, full rough)
  {
    rhi::TextureDesc texDesc;
    texDesc.width = texDesc.height = 1;
    texDesc.format                 = rhi::TextureFormat::Rgba8;
    texDesc.createFlags            = gfx::rhi::TextureCreateFlag::TransferDst;
    texDesc.initialLayout          = rhi::ResourceLayout::ShaderReadOnly;
    texDesc.debugName              = "default_black_texture";

    auto texture = m_device->createTexture(texDesc);

    // black color
    uint32_t blackPixel = 0xFF'00'00'00;  // Alpha = 1, RGB = 0
    m_device->updateTexture(texture.get(), &blackPixel, sizeof(blackPixel));

    m_defaultBlackTexture = m_resourceManager->addTexture(std::move(texture), "default_black_texture");
  }
}

void FrameResources::createDefaultSampler_() {
  rhi::SamplerDesc samplerDesc;
  samplerDesc.minFilter     = rhi::TextureFilter::Linear;
  samplerDesc.magFilter     = rhi::TextureFilter::Linear;
  samplerDesc.addressModeU  = rhi::TextureAddressMode::Repeat;
  samplerDesc.addressModeV  = rhi::TextureAddressMode::Repeat;
  samplerDesc.addressModeW  = rhi::TextureAddressMode::Repeat;
  samplerDesc.maxAnisotropy = 16.0f;

  auto sampler     = m_device->createSampler(samplerDesc);
  m_defaultSampler = m_resourceManager->addSampler(std::move(sampler), "default_sampler");
}

void FrameResources::createSamplerDescriptorSet_() {
  rhi::DescriptorSetLayoutDesc        samplerLayoutDesc;
  rhi::DescriptorSetLayoutBindingDesc samplerBindingDesc;
  samplerBindingDesc.binding    = 0;
  samplerBindingDesc.type       = rhi::ShaderBindingType::Sampler;
  samplerBindingDesc.stageFlags = rhi::ShaderStageFlag::Fragment;
  samplerLayoutDesc.bindings.push_back(samplerBindingDesc);

  auto samplerSetLayout = m_device->createDescriptorSetLayout(samplerLayoutDesc);
  auto samplerSetLayoutPtr
      = m_resourceManager->addDescriptorSetLayout(std::move(samplerSetLayout), "default_sampler_layout");

  auto samplerDescriptorSet = m_device->createDescriptorSet(samplerSetLayoutPtr);
  samplerDescriptorSet->setSampler(0, m_defaultSampler);

  m_defaultSamplerDescriptorSet
      = m_resourceManager->addDescriptorSet(std::move(samplerDescriptorSet), "default_sampler_descriptor_set");
}

void FrameResources::createRenderTargets_(RenderTargets& targets, const math::Dimension2i& dimensions) {
  auto width  = dimensions.width() != 0 ? dimensions.width() : 1;
  auto height = dimensions.height() != 0 ? dimensions.height() : 1;

  rhi::TextureDesc colorDesc;
  colorDesc.width       = width;
  colorDesc.height      = height;
  colorDesc.format      = rhi::TextureFormat::Bgra8;
  colorDesc.createFlags = rhi::TextureCreateFlag::Rtv | rhi::TextureCreateFlag::TransferSrc
                        | rhi::TextureCreateFlag::TransferDst /*| rhi::TextureCreateFlag::ShaderResource*/;
  colorDesc.initialLayout = rhi::ResourceLayout::ColorAttachment;
  colorDesc.debugName     = "color_buffer";

  targets.colorBuffer = m_device->createTexture(colorDesc);

  rhi::TextureDesc depthDesc;
  depthDesc.width         = width;
  depthDesc.height        = height;
  depthDesc.format        = rhi::TextureFormat::D24S8;
  depthDesc.createFlags   = rhi::TextureCreateFlag::Dsv;
  depthDesc.initialLayout = rhi::ResourceLayout::DepthStencilAttachment;
  depthDesc.debugName     = "depth_buffer";

  targets.depthBuffer = m_device->createTexture(depthDesc);
}

void FrameResources::updateViewResources_(const RenderContext& context) {
  CPU_ZONE_NC("Update View Buffer", color::YELLOW);
  auto& registry = context.scene->getEntityRegistry();
  auto  view     = registry.view<ecs::Transform, ecs::Camera, ecs::CameraMatrices>();

  if (view.begin() == view.end()) {
    GlobalLogger::Log(LogLevel::Warning, "No main Camera exists!");
    return;
  }

  auto  entity       = *view.begin();
  auto& transform    = view.get<ecs::Transform>(entity);
  auto& cameraMatrix = view.get<ecs::CameraMatrices>(entity);

  if (!m_viewUniformBuffer) {
    rhi::BufferDesc viewUboDesc;
    viewUboDesc.size = alignConstantBufferSize(sizeof(math::Matrix4f<>) * 6 + sizeof(math::Vector3f) + sizeof(float));
    viewUboDesc.createFlags = rhi::BufferCreateFlag::CpuAccess | rhi::BufferCreateFlag::ConstantBuffer;
    viewUboDesc.type        = rhi::BufferType::Dynamic;
    viewUboDesc.debugName   = "view_buffer";

    auto viewBuffer     = m_device->createBuffer(viewUboDesc);
    m_viewUniformBuffer = m_resourceManager->addBuffer(std::move(viewBuffer), "view_buffer");

    // Create descriptor set as well
    auto viewDescriptorSet = m_device->createDescriptorSet(m_viewDescriptorSetLayout);
    viewDescriptorSet->setUniformBuffer(0, m_viewUniformBuffer);
    m_viewDescriptorSet = m_resourceManager->addDescriptorSet(std::move(viewDescriptorSet), "view_descriptor_set");
  }

  struct ViewData {
    math::Matrix4f<> view;
    math::Matrix4f<> projection;
    math::Matrix4f<> viewProjection;
    math::Matrix4f<> invView;
    math::Matrix4f<> invProjection;
    math::Matrix4f<> invViewProjection;
    math::Vector3f  eyePosition;
    float            padding;
  } viewData;

  viewData.view              = cameraMatrix.view;
  viewData.projection        = cameraMatrix.projection;
  viewData.viewProjection    = cameraMatrix.view * cameraMatrix.projection;
  viewData.invView           = cameraMatrix.view.inverse();
  viewData.invProjection     = cameraMatrix.projection.inverse();
  viewData.invViewProjection = viewData.viewProjection.inverse();
  viewData.eyePosition       = transform.translation;
  viewData.padding           = 0.0f;

  m_device->updateBuffer(m_viewUniformBuffer, &viewData, sizeof(viewData));
}

void FrameResources::updateModelList_(const RenderContext& context) {
  bool                             needRebuildRenderArray = false;
  std::unordered_set<entt::entity> currentEntityIds;
  std::unordered_set<ecs::Material*> activeMaterials;

  auto& registry = context.scene->getEntityRegistry();
  auto  view     = registry.view<ecs::Transform, ecs::RenderModel*>();

  for (auto entity : view) {
    currentEntityIds.insert(entity);
    auto& transform   = view.get<ecs::Transform>(entity);
    auto& renderModel = view.get<ecs::RenderModel*>(entity);

    for (const auto& renderMesh : renderModel->renderMeshes) {
      if (renderMesh->material) {
        activeMaterials.insert(renderMesh->material);
      }
    }

    auto it = m_modelsMap.find(entity);
    if (it != m_modelsMap.end()) {
      if (transform.isDirty) {
              it->second.transform   = transform;
      it->second.modelMatrix = ecs::calculateTransformMatrix(transform);
        it->second.isDirty     = true;
      }
    } else {
      ModelInstance instance;
      instance.model       = renderModel;
          instance.transform   = transform;
    instance.modelMatrix = ecs::calculateTransformMatrix(transform);
      instance.entityId    = entity;
      instance.isDirty     = true;

      if (!renderModel->renderMeshes.empty() && renderModel->renderMeshes[0]->material) {
        instance.materialId = reinterpret_cast<uintptr_t>(renderModel->renderMeshes[0]->material);
      }

      m_modelsMap[entity]    = instance;
      needRebuildRenderArray = true;
    }
  }

  for (auto it = m_modelsMap.begin(); it != m_modelsMap.end();) {
    if (!currentEntityIds.contains(it->first)) {
      it                     = m_modelsMap.erase(it);
      needRebuildRenderArray = true;
    } else {
      ++it;
    }
  }

  std::vector<ecs::Material*> materialsToRemove;
  for (const auto& [material, cache] : m_materialParamCache) {
    if (!activeMaterials.contains(material)) {
      materialsToRemove.push_back(material);
    }
  }

  for (auto material : materialsToRemove) {
    GlobalLogger::Log(LogLevel::Debug,
                      "Removing cached material parameters for deleted material at address: "
                          + std::to_string(reinterpret_cast<uintptr_t>(material)));
    m_materialParamCache.erase(material);
  }

  if (needRebuildRenderArray) {
    m_sortedModels.clear();
    m_sortedModels.reserve(m_modelsMap.size());

    for (auto& pair : m_modelsMap) {
      m_sortedModels.push_back(&pair.second);
    }

    sortModelsByMaterial_();
  }
}

void FrameResources::sortModelsByMaterial_() {
  std::sort(m_sortedModels.begin(), m_sortedModels.end(), [](const ModelInstance* a, const ModelInstance* b) {
    return a->materialId < b->materialId;
  });
}

void FrameResources::clearInternalDirtyFlags_() {
  for (auto& instance : m_sortedModels) {
    instance->isDirty = false;
  }
}

void FrameResources::clearEntityDirtyFlags_(const RenderContext& context) {
  if (context.scene) {
    auto& registry = context.scene->getEntityRegistry();
    auto  view     = registry.view<ecs::Transform>();

    for (auto entity : view) {
      auto& transform = view.get<ecs::Transform>(entity);
      if (transform.isDirty) {
        transform.isDirty = false;
      }
    }
  }
}

}  // namespace renderer
}  // namespace gfx
}  // namespace arise