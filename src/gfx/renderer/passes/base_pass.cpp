#include "gfx/renderer/passes/base_pass.h"

#include "ecs/components/material.h"
#include "ecs/components/render_model.h"
#include "ecs/components/vertex.h"
#include "gfx/renderer/frame_resources.h"
#include "gfx/renderer/render_resource_manager.h"
#include "gfx/rhi/interface/buffer.h"
#include "gfx/rhi/interface/descriptor.h"
#include "gfx/rhi/interface/pipeline.h"
#include "gfx/rhi/shader_manager.h"
#include "profiler/profiler.h"

namespace arise {
namespace gfx {
namespace renderer {
void BasePass::initialize(rhi::Device*           device,
                          RenderResourceManager* resourceManager,
                          FrameResources*        frameResources,
                          rhi::ShaderManager*    shaderManager) {
  m_device          = device;
  m_resourceManager = resourceManager;
  m_frameResources  = frameResources;
  m_shaderManager   = shaderManager;

  if (shaderManager) {
    m_vertexShader = shaderManager->getShader(m_vertexShaderPath_);
    m_pixelShader  = shaderManager->getShader(m_pixelShaderPath_);
  } else {
    GlobalLogger::Log(LogLevel::Error, "ShaderManager not found");
  }

  setupRenderPass_();
}

void BasePass::resize(const math::Dimension2i& newDimension) {
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

  createFramebuffer_(newDimension);
}

void BasePass::prepareFrame(const RenderContext& context) {
  CPU_ZONE_NC("BasePass::prepareFrame", color::YELLOW);

  std::unordered_map<RenderModel*, std::vector<math::Matrix4f<>>> currentFrameInstances;
  std::unordered_map<RenderModel*, bool>                          modelDirtyFlags;

  for (const auto& instance : m_frameResources->getModels()) {
    currentFrameInstances[instance->model].push_back(instance->modelMatrix);

    if (instance->isDirty) {
      modelDirtyFlags[instance->model] = true;
    }
  }

  for (auto& [model, matrices] : currentFrameInstances) {
    auto& cache = m_instanceBufferCache[model];

    bool needsUpdate = cache.instanceBuffer == nullptr ||   // Buffer not created yet
                       matrices.size() > cache.capacity ||  // Need more space
                       matrices.size() != cache.count ||    // Count changed
                       modelDirtyFlags[model];              // Model was modified

    if (needsUpdate) {
      CPU_ZONE_NC("Update Instance Buffers", color::YELLOW);
      updateInstanceBuffer_(model, matrices, cache);
    }
  }

  cleanupUnusedBuffers_(currentFrameInstances);

  prepareDrawCalls_(context);
}

void BasePass::render(const RenderContext& context) {
  CPU_ZONE_NC("BasePass::render", color::ORANGE);

  auto commandBuffer = context.commandBuffer.get();
  if (!commandBuffer || !m_renderPass || m_framebuffers.empty()) {
    return;
  }

  GPU_ZONE_NC(commandBuffer, "Base Pass", color::ORANGE);

  std::vector<rhi::ClearValue> clearValues;

  rhi::ClearValue colorClear;
  colorClear.color[0] = 0.0f;
  colorClear.color[1] = 0.0f;
  colorClear.color[2] = 0.0f;
  colorClear.color[3] = 1.0f;
  clearValues.push_back(colorClear);

  rhi::ClearValue depthClear;
  depthClear.depthStencil.depth   = 1.0f;
  depthClear.depthStencil.stencil = 0;
  clearValues.push_back(depthClear);

  uint32_t currentIndex = context.currentImageIndex;
  if (currentIndex >= m_framebuffers.size()) {
    GlobalLogger::Log(LogLevel::Error, "Invalid framebuffer index");
    return;
  }

  rhi::Framebuffer* currentFramebuffer = m_framebuffers[currentIndex];

  commandBuffer->beginRenderPass(m_renderPass, currentFramebuffer, clearValues);

  commandBuffer->setViewport(m_viewport);
  commandBuffer->setScissor(m_scissor);

  {
    CPU_ZONE_NC("Draw Models", color::GREEN);
    for (const auto& drawData : m_drawData) {
      commandBuffer->setPipeline(drawData.pipeline);

      if (m_frameResources->getViewDescriptorSet()) {
        commandBuffer->bindDescriptorSet(0, m_frameResources->getViewDescriptorSet());
      }

      if (drawData.modelMatrixDescriptorSet) {
        commandBuffer->bindDescriptorSet(1, drawData.modelMatrixDescriptorSet);
      }

      if (m_frameResources->getLightDescriptorSet()) {
        commandBuffer->bindDescriptorSet(2, m_frameResources->getLightDescriptorSet());
      }

      if (drawData.materialDescriptorSet) {
        commandBuffer->bindDescriptorSet(3, drawData.materialDescriptorSet);
      }

      if (m_frameResources->getDefaultSamplerDescriptorSet()) {
        commandBuffer->bindDescriptorSet(4, m_frameResources->getDefaultSamplerDescriptorSet());
      }

      commandBuffer->bindVertexBuffer(0, drawData.vertexBuffer);
      commandBuffer->bindVertexBuffer(1, drawData.instanceBuffer);
      commandBuffer->bindIndexBuffer(drawData.indexBuffer, 0, true);

      commandBuffer->drawIndexedInstanced(drawData.indexCount, drawData.instanceCount, 0, 0, 0);
    }
  }
  commandBuffer->endRenderPass();
}

void BasePass::clearSceneResources() {
  m_instanceBufferCache.clear();
  m_materialCache.clear();
  m_drawData.clear();
  GlobalLogger::Log(LogLevel::Info, "Base pass resources cleared for scene switch");
}

void BasePass::cleanup() {
  m_instanceBufferCache.clear();
  m_materialCache.clear();
  m_drawData.clear();
  m_renderPass = nullptr;
  m_framebuffers.clear();
  m_vertexShader = nullptr;
  m_pixelShader  = nullptr;
}

void BasePass::setupRenderPass_() {
  rhi::RenderPassDesc renderPassDesc;

  // Color attachment
  rhi::RenderPassAttachmentDesc colorAttachmentDesc;
  colorAttachmentDesc.format        = rhi::TextureFormat::Bgra8;
  colorAttachmentDesc.samples       = rhi::MSAASamples::Count1;
  colorAttachmentDesc.loadStoreOp   = rhi::AttachmentLoadStoreOp::ClearStore;
  colorAttachmentDesc.initialLayout = rhi::ResourceLayout::ColorAttachment;
  colorAttachmentDesc.finalLayout   = rhi::ResourceLayout::ColorAttachment;
  renderPassDesc.colorAttachments.push_back(colorAttachmentDesc);

  // Depth attachment
  rhi::RenderPassAttachmentDesc depthAttachmentDesc;
  depthAttachmentDesc.format             = rhi::TextureFormat::D24S8;
  depthAttachmentDesc.samples            = rhi::MSAASamples::Count1;
  depthAttachmentDesc.loadStoreOp        = rhi::AttachmentLoadStoreOp::ClearStore;
  depthAttachmentDesc.stencilLoadStoreOp = rhi::AttachmentLoadStoreOp::ClearStore;
  depthAttachmentDesc.initialLayout      = rhi::ResourceLayout::DepthStencilAttachment;
  depthAttachmentDesc.finalLayout        = rhi::ResourceLayout::DepthStencilAttachment;
  renderPassDesc.depthStencilAttachment  = depthAttachmentDesc;
  renderPassDesc.hasDepthStencil         = true;

  auto renderPass = m_device->createRenderPass(renderPassDesc);
  m_renderPass    = m_resourceManager->addRenderPass(std::move(renderPass), "base_pass_render_pass");
}

void BasePass::createFramebuffer_(const math::Dimension2i& dimension) {
  if (!m_renderPass) {
    GlobalLogger::Log(LogLevel::Error, "Render pass must be created before framebuffer");
    return;
  }

  m_framebuffers.clear();

  uint32_t framesCount = m_frameResources->getFramesCount();

  for (uint32_t i = 0; i < framesCount; i++) {
    std::string framebufferKey = "base_pass_framebuffer_" + std::to_string(i);

    rhi::Framebuffer* existingFramebuffer = m_resourceManager->getFramebuffer(framebufferKey);
    if (existingFramebuffer) {
      m_resourceManager->removeFramebuffer(framebufferKey);
    }

    auto& renderTargets = m_frameResources->getRenderTargets(i);

    rhi::FramebufferDesc framebufferDesc;
    framebufferDesc.width  = dimension.width();
    framebufferDesc.height = dimension.height();
    framebufferDesc.colorAttachments.push_back(renderTargets.colorBuffer.get());
    framebufferDesc.depthStencilAttachment = renderTargets.depthBuffer.get();
    framebufferDesc.hasDepthStencil        = true;
    framebufferDesc.renderPass             = m_renderPass;

    auto framebuffer    = m_device->createFramebuffer(framebufferDesc);
    auto framebufferPtr = m_resourceManager->addFramebuffer(std::move(framebuffer), framebufferKey);

    m_framebuffers.push_back(framebufferPtr);
  }
}

void BasePass::updateInstanceBuffer_(RenderModel*                         model,
                                     const std::vector<math::Matrix4f<>>& matrices,
                                     ModelBufferCache&                    cache) {
  if (!cache.instanceBuffer || matrices.size() > cache.capacity) {
    // If we already have a buffer, we'll let the resource manager handle freeing it

    // Create a new buffer with some growth room
    uint32_t newCapacity = std::max(static_cast<uint32_t>(matrices.size() * 1.5), 8u);

    std::string bufferKey = "instance_buffer_" + std::to_string(reinterpret_cast<uintptr_t>(model));

    rhi::BufferDesc bufferDesc;
    bufferDesc.size        = newCapacity * sizeof(math::Matrix4f<>);
    bufferDesc.createFlags = rhi::BufferCreateFlag::InstanceBuffer;
    bufferDesc.type        = rhi::BufferType::Dynamic;
    bufferDesc.stride      = sizeof(math::Matrix4f<>);
    bufferDesc.debugName   = bufferKey;

    auto buffer          = m_device->createBuffer(bufferDesc);
    cache.instanceBuffer = m_resourceManager->addBuffer(std::move(buffer), bufferKey);
    cache.capacity       = newCapacity;
  }

  if (cache.instanceBuffer && !matrices.empty()) {
    m_device->updateBuffer(cache.instanceBuffer, matrices.data(), matrices.size() * sizeof(math::Matrix4f<>));
  }

  cache.count = static_cast<uint32_t>(matrices.size());
}

void BasePass::prepareDrawCalls_(const RenderContext& context) {
  m_drawData.clear();

  auto viewLayout        = m_frameResources->getViewDescriptorSetLayout();
  auto lightLayout       = m_frameResources->getLightDescriptorSetLayout();
  auto modelMatrixLayout = m_frameResources->getModelMatrixDescriptorSetLayout();
  auto materialLayout    = m_frameResources->getMaterialDescriptorSetLayout();
  auto samplerLayout     = m_frameResources->getDefaultSamplerDescriptorSet()->getLayout();

  for (const auto& [model, cache] : m_instanceBufferCache) {
    if (cache.count == 0) {
      continue;
    }

    for (const auto& renderMesh : model->renderMeshes) {
      if (!renderMesh->material) {
        GlobalLogger::Log(LogLevel::Debug, "RenderMesh has null material, skipping");
        continue;
      }

      rhi::DescriptorSet* materialDescriptorSet = getOrCreateMaterialDescriptorSet_(renderMesh->material);

      if (!materialDescriptorSet) {
        GlobalLogger::Log(LogLevel::Debug, "Could not create valid material descriptor set, skipping");
        continue;
      }

      std::string pipelineKey
          = "base_pipeline_" + std::to_string(reinterpret_cast<uintptr_t>(renderMesh->gpuMesh->vertexBuffer));

      rhi::GraphicsPipeline* pipeline = m_resourceManager->getPipeline(pipelineKey);

      if (!pipeline) {
        rhi::GraphicsPipelineDesc pipelineDesc;

        pipelineDesc.shaders.push_back(m_vertexShader);
        pipelineDesc.shaders.push_back(m_pixelShader);

        rhi::VertexInputBindingDesc vertexBinding;
        vertexBinding.binding   = 0;
        vertexBinding.stride    = sizeof(Vertex);
        vertexBinding.inputRate = rhi::VertexInputRate::Vertex;
        pipelineDesc.vertexBindings.push_back(vertexBinding);

        rhi::VertexInputBindingDesc instanceBinding;
        instanceBinding.binding   = 1;
        instanceBinding.stride    = sizeof(math::Matrix4f<>);
        instanceBinding.inputRate = rhi::VertexInputRate::Instance;
        pipelineDesc.vertexBindings.push_back(instanceBinding);

        rhi::VertexInputAttributeDesc positionAttr;
        positionAttr.location     = 0;
        positionAttr.binding      = 0;
        positionAttr.format       = rhi::TextureFormat::Rgb32f;
        positionAttr.offset       = offsetof(Vertex, position);
        positionAttr.semanticName = "POSITION";
        pipelineDesc.vertexAttributes.push_back(positionAttr);

        rhi::VertexInputAttributeDesc uvAttr;
        uvAttr.location     = 1;
        uvAttr.binding      = 0;
        uvAttr.format       = rhi::TextureFormat::Rg32f;
        uvAttr.offset       = offsetof(Vertex, texCoords);
        uvAttr.semanticName = "TEXCOORD";
        pipelineDesc.vertexAttributes.push_back(uvAttr);

        rhi::VertexInputAttributeDesc normalAttr;
        normalAttr.location     = 2;
        normalAttr.binding      = 0;
        normalAttr.format       = rhi::TextureFormat::Rgb32f;
        normalAttr.offset       = offsetof(Vertex, normal);
        normalAttr.semanticName = "NORMAL";
        pipelineDesc.vertexAttributes.push_back(normalAttr);

        rhi::VertexInputAttributeDesc tangentAttr;
        tangentAttr.location     = 3;
        tangentAttr.binding      = 0;
        tangentAttr.format       = rhi::TextureFormat::Rgb32f;
        tangentAttr.offset       = offsetof(Vertex, tangent);
        tangentAttr.semanticName = "TANGENT";
        pipelineDesc.vertexAttributes.push_back(tangentAttr);

        rhi::VertexInputAttributeDesc bitangentAttr;
        bitangentAttr.location     = 4;
        bitangentAttr.binding      = 0;
        bitangentAttr.format       = rhi::TextureFormat::Rgb32f;
        bitangentAttr.offset       = offsetof(Vertex, bitangent);
        bitangentAttr.semanticName = "BITANGENT";
        pipelineDesc.vertexAttributes.push_back(bitangentAttr);

        rhi::VertexInputAttributeDesc colorAttr;
        colorAttr.location     = 5;
        colorAttr.binding      = 0;
        colorAttr.format       = rhi::TextureFormat::Rgba32f;
        colorAttr.offset       = offsetof(Vertex, color);
        colorAttr.semanticName = "COLOR";
        pipelineDesc.vertexAttributes.push_back(colorAttr);

        for (uint32_t i = 0; i < 4; i++) {
          rhi::VertexInputAttributeDesc matrixCol;
          matrixCol.location     = 6 + i;
          matrixCol.binding      = 1;
          matrixCol.format       = rhi::TextureFormat::Rgba32f;
          matrixCol.offset       = i * 16;
          matrixCol.semanticName = "INSTANCE";
          pipelineDesc.vertexAttributes.push_back(matrixCol);
        }

        pipelineDesc.inputAssembly.topology               = rhi::PrimitiveType::Triangles;
        pipelineDesc.inputAssembly.primitiveRestartEnable = false;

        pipelineDesc.rasterization.polygonMode     = rhi::PolygonMode::Fill;
        pipelineDesc.rasterization.cullMode        = rhi::CullMode::Back;
        pipelineDesc.rasterization.frontFace       = rhi::FrontFace::Ccw;
        pipelineDesc.rasterization.depthBiasEnable = false;
        pipelineDesc.rasterization.lineWidth       = 1.0f;

        pipelineDesc.depthStencil.depthTestEnable  = true;
        pipelineDesc.depthStencil.depthWriteEnable = true;
        pipelineDesc.depthStencil.depthCompareOp   = rhi::CompareOp::Less;

        pipelineDesc.depthStencil.stencilTestEnable = false;

        rhi::ColorBlendAttachmentDesc blendAttachment;
        blendAttachment.blendEnable         = true;
        blendAttachment.srcColorBlendFactor = rhi::BlendFactor::SrcAlpha;
        blendAttachment.dstColorBlendFactor = rhi::BlendFactor::OneMinusSrcAlpha;
        blendAttachment.colorBlendOp        = rhi::BlendOp::Add;
        blendAttachment.srcAlphaBlendFactor = rhi::BlendFactor::One;
        blendAttachment.dstAlphaBlendFactor = rhi::BlendFactor::OneMinusSrcAlpha;
        blendAttachment.alphaBlendOp        = rhi::BlendOp::Add;
        blendAttachment.colorWriteMask      = rhi::ColorMask::All;
        pipelineDesc.colorBlend.attachments.push_back(blendAttachment);

        pipelineDesc.multisample.rasterizationSamples = rhi::MSAASamples::Count1;

        pipelineDesc.setLayouts.push_back(viewLayout);
        pipelineDesc.setLayouts.push_back(modelMatrixLayout);
        pipelineDesc.setLayouts.push_back(lightLayout);
        pipelineDesc.setLayouts.push_back(materialLayout);
        pipelineDesc.setLayouts.push_back(samplerLayout);

        pipelineDesc.renderPass = m_renderPass;

        auto pipelineObj = m_device->createGraphicsPipeline(pipelineDesc);
        pipeline         = m_resourceManager->addPipeline(std::move(pipelineObj), pipelineKey);

        m_shaderManager->registerPipelineForShader(pipeline, m_vertexShaderPath_);
        m_shaderManager->registerPipelineForShader(pipeline, m_pixelShaderPath_);
      }

      DrawData drawData;
      drawData.pipeline                 = pipeline;
      drawData.modelMatrixDescriptorSet = m_frameResources->getOrCreateModelMatrixDescriptorSet(renderMesh);
      drawData.materialDescriptorSet    = materialDescriptorSet;
      drawData.vertexBuffer             = renderMesh->gpuMesh->vertexBuffer;
      drawData.indexBuffer              = renderMesh->gpuMesh->indexBuffer;
      drawData.instanceBuffer           = cache.instanceBuffer;
      drawData.indexCount               = renderMesh->gpuMesh->indexBuffer->getDesc().size / sizeof(uint32_t);
      drawData.instanceCount            = cache.count;

      m_drawData.push_back(drawData);
    }
  }
}

void BasePass::cleanupUnusedBuffers_(
    const std::unordered_map<RenderModel*, std::vector<math::Matrix4f<>>>& currentFrameInstances) {
  std::vector<RenderModel*> modelsToRemove;
  for (const auto& [model, cache] : m_instanceBufferCache) {
    if (!currentFrameInstances.contains(model)) {
      modelsToRemove.push_back(model);
    }
  }

  for (auto model : modelsToRemove) {
    m_instanceBufferCache.erase(model);
  }

  std::unordered_set<Material*> activeMaterials;

  for (const auto& [model, matrices] : currentFrameInstances) {
    for (const auto& renderMesh : model->renderMeshes) {
      if (renderMesh->material) {
        activeMaterials.insert(renderMesh->material);
      }
    }
  }

  std::vector<Material*> materialsToRemove;
  for (const auto& [material, cache] : m_materialCache) {
    if (!activeMaterials.contains(material)) {
      materialsToRemove.push_back(material);
    }
  }

  for (auto material : materialsToRemove) {
    GlobalLogger::Log(LogLevel::Debug,
                      "Removing cached material parameters for deleted material at address: "
                          + std::to_string(reinterpret_cast<uintptr_t>(material)));
    m_materialCache.erase(material);
  }
}

rhi::DescriptorSet* BasePass::getOrCreateMaterialDescriptorSet_(Material* material) {
  if (!material) {
    GlobalLogger::Log(LogLevel::Error, "Material is null");
    return nullptr;
  }

  auto it = m_materialCache.find(material);
  if (it != m_materialCache.end() && it->second.descriptorSet) {
    return it->second.descriptorSet;
  }

  auto materialLayout = m_resourceManager->getDescriptorSetLayout("material_layout");
  if (!materialLayout) {
    GlobalLogger::Log(LogLevel::Error, "Material descriptor set layout not found");
    return nullptr;
  }

  std::string materialKey = "material_" + std::to_string(reinterpret_cast<uintptr_t>(material));

  auto descriptorSetPtr = m_resourceManager->getDescriptorSet(materialKey);
  if (!descriptorSetPtr) {
    auto descriptorSet = m_device->createDescriptorSet(materialLayout);
    
    // Update the descriptor set BEFORE adding it to the resource manager
    rhi::Buffer* paramBuffer = m_frameResources->getOrCreateMaterialParamBuffer(material);
    if (paramBuffer) {
      descriptorSet->setUniformBuffer(0, paramBuffer);
    }

    std::vector<std::string> textureNames = {"albedo", "normal_map", "metallic_roughness"};
    uint32_t                 binding      = 1;

    bool allTexturesValid = true;

    for (const auto& textureName : textureNames) {
      rhi::Texture* texture = nullptr;

      auto textureIt = material->textures.find(textureName);
      if (textureIt != material->textures.end() && textureIt->second) {
        texture = textureIt->second;
      }

      if (!texture) {
        if (textureName == "albedo") {
          texture = m_frameResources->getDefaultWhiteTexture();
        } else if (textureName == "normal_map") {
          texture = m_frameResources->getDefaultNormalTexture();
        } else if (textureName == "metallic_roughness") {
          texture = m_frameResources->getDefaultBlackTexture();
        }

        GlobalLogger::Log(LogLevel::Debug,
                          "Using fallback texture for '" + textureName + "' in material: " + material->materialName);
      }

      if (!texture) {
        GlobalLogger::Log(
            LogLevel::Error,
            "No texture available (including fallback) for '" + textureName + "' in material: " + material->materialName);
        allTexturesValid = false;
        break;
      }

      descriptorSet->setTexture(binding, texture);
      binding++;
    }

    if (allTexturesValid) {
      descriptorSetPtr = m_resourceManager->addDescriptorSet(std::move(descriptorSet), materialKey);
    } else {
      GlobalLogger::Log(LogLevel::Warning,
                        "Material descriptor set creation failed due to invalid textures: " + material->materialName);
      return nullptr;
    }
  }

  m_materialCache[material].descriptorSet = descriptorSetPtr;
  return descriptorSetPtr;
}
}  // namespace renderer
}  // namespace gfx
}  // namespace arise
