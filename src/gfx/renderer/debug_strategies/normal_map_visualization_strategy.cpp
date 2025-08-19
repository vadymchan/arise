#include "gfx/renderer/debug_strategies/normal_map_visualization_strategy.h"

#include "ecs/components/material.h"
#include "ecs/components/render_model.h"
#include "ecs/components/vertex.h"
#include "gfx/renderer/frame_resources.h"
#include "gfx/renderer/render_resource_manager.h"
#include "gfx/rhi/interface/buffer.h"
#include "gfx/rhi/interface/descriptor.h"
#include "gfx/rhi/interface/pipeline.h"
#include "gfx/rhi/interface/render_pass.h"
#include "gfx/rhi/shader_manager.h"
#include "gfx/rhi/shader_reflection/pipeline_layout_builder.h"
#include "gfx/rhi/shader_reflection/pipeline_layout_manager.h"
#include "gfx/rhi/shader_reflection/pipeline_utils.h"
#include "gfx/rhi/shader_reflection/shader_reflection_utils.h"
#include "gfx/rhi/shader_reflection/vertex_input_builder.h"

namespace arise {
namespace gfx {
namespace renderer {

void NormalMapVisualizationStrategy::initialize(rhi::Device*           device,
                                                RenderResourceManager* resourceManager,
                                                FrameResources*        frameResources,
                                                rhi::ShaderManager*    shaderManager) {
  m_device          = device;
  m_resourceManager = resourceManager;
  m_frameResources  = frameResources;
  m_shaderManager   = shaderManager;

  m_vertexShader = m_shaderManager->getShader(m_vertexShaderPath_);
  m_pixelShader  = m_shaderManager->getShader(m_pixelShaderPath_);

  rhi::DescriptorSetLayoutDesc        materialLayoutDesc;
  rhi::DescriptorSetLayoutBindingDesc textureBindingDesc;
  textureBindingDesc.binding    = 0;
  textureBindingDesc.type       = rhi::ShaderBindingType::TextureSrv;
  textureBindingDesc.stageFlags = rhi::ShaderStageFlag::Fragment;
  materialLayoutDesc.bindings.push_back(textureBindingDesc);

  auto materialSetLayout        = m_device->createDescriptorSetLayout(materialLayoutDesc);
  m_materialDescriptorSetLayout = m_resourceManager->getDescriptorSetLayout("normal_map_material_layout");
  if (!m_materialDescriptorSetLayout) {
    m_materialDescriptorSetLayout
        = m_resourceManager->addDescriptorSetLayout(std::move(materialSetLayout), "normal_map_material_layout");
  }

  setupRenderPass_();
}

void NormalMapVisualizationStrategy::resize(const math::Dimension2i& newDimension) {
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

  createFramebuffers_(newDimension);
}

void NormalMapVisualizationStrategy::prepareFrame(const RenderContext& context) {
  std::unordered_map<ecs::RenderModel*, std::vector<math::Matrix4f<>>> currentFrameInstances;
  std::unordered_map<ecs::RenderModel*, bool>                          modelDirtyFlags;

  for (const auto& instance : m_frameResources->getModels()) {
    currentFrameInstances[instance->model].push_back(instance->modelMatrix);

    if (instance->isDirty && !modelDirtyFlags[instance->model]) {
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
      updateInstanceBuffer_(model, matrices, cache);
    }
  }

  cleanupUnusedBuffers_(currentFrameInstances);
  prepareDrawCalls_(context);
}

void NormalMapVisualizationStrategy::render(const RenderContext& context) {
  auto commandBuffer = context.commandBuffer.get();
  if (!commandBuffer || !m_renderPass || m_framebuffers.empty()) {
    return;
  }

  uint32_t currentIndex = context.currentImageIndex;
  if (currentIndex >= m_framebuffers.size()) {
    LOG_ERROR("Invalid framebuffer index");
    return;
  }

  rhi::Framebuffer* currentFramebuffer = m_framebuffers[currentIndex];

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

  commandBuffer->beginRenderPass(m_renderPass, currentFramebuffer, clearValues);

  commandBuffer->setViewport(m_viewport);
  commandBuffer->setScissor(m_scissor);

  for (const auto& drawData : m_drawData) {
    commandBuffer->setPipeline(drawData.pipeline);

    if (drawData.pipeline->hasBindingSlot(0) && m_frameResources->getViewDescriptorSet()) {
      commandBuffer->bindDescriptorSet(0, m_frameResources->getViewDescriptorSet());
    }

    if (drawData.pipeline->hasBindingSlot(1) && drawData.modelMatrixDescriptorSet) {
      commandBuffer->bindDescriptorSet(1, drawData.modelMatrixDescriptorSet);
    }

    if (drawData.pipeline->hasBindingSlot(2) && drawData.materialDescriptorSet) {
      commandBuffer->bindDescriptorSet(2, drawData.materialDescriptorSet);
    }

    if (drawData.pipeline->hasBindingSlot(3) && m_frameResources->getDefaultSamplerDescriptorSet()) {
      commandBuffer->bindDescriptorSet(3, m_frameResources->getDefaultSamplerDescriptorSet());
    }

    commandBuffer->bindVertexBuffer(0, drawData.vertexBuffer);
    commandBuffer->bindVertexBuffer(1, drawData.instanceBuffer);
    commandBuffer->bindIndexBuffer(drawData.indexBuffer, 0, true);

    commandBuffer->drawIndexedInstanced(drawData.indexCount, drawData.instanceCount, 0, 0, 0);
  }

  commandBuffer->endRenderPass();
}

void NormalMapVisualizationStrategy::clearSceneResources() {
  m_instanceBufferCache.clear();
  m_materialCache.clear();
  m_drawData.clear();
  LOG_INFO("Normal map visualization strategy resources cleared for scene switch");
}

void NormalMapVisualizationStrategy::cleanup() {
  m_instanceBufferCache.clear();
  m_materialCache.clear();
  m_drawData.clear();
  m_renderPass = nullptr;
  m_framebuffers.clear();
  m_vertexShader = nullptr;
  m_pixelShader  = nullptr;
  m_layoutManager.cleanup();
}

void NormalMapVisualizationStrategy::setupRenderPass_() {
  rhi::RenderPassDesc renderPassDesc;

  rhi::RenderPassAttachmentDesc colorAttachmentDesc;
  colorAttachmentDesc.format        = rhi::TextureFormat::Bgra8;
  colorAttachmentDesc.samples       = rhi::MSAASamples::Count1;
  colorAttachmentDesc.loadStoreOp   = rhi::AttachmentLoadStoreOp::ClearStore;
  colorAttachmentDesc.initialLayout = rhi::ResourceLayout::ColorAttachment;
  colorAttachmentDesc.finalLayout   = rhi::ResourceLayout::ColorAttachment;
  renderPassDesc.colorAttachments.push_back(colorAttachmentDesc);

  rhi::RenderPassAttachmentDesc depthAttachmentDesc;
  depthAttachmentDesc.format             = rhi::TextureFormat::D24S8;
  depthAttachmentDesc.samples            = rhi::MSAASamples::Count1;
  depthAttachmentDesc.loadStoreOp        = rhi::AttachmentLoadStoreOp::ClearStore;
  depthAttachmentDesc.stencilLoadStoreOp = rhi::AttachmentLoadStoreOp::DontcareDontcare;
  depthAttachmentDesc.initialLayout      = rhi::ResourceLayout::DepthStencilAttachment;
  depthAttachmentDesc.finalLayout        = rhi::ResourceLayout::DepthStencilAttachment;
  renderPassDesc.depthStencilAttachment  = depthAttachmentDesc;
  renderPassDesc.hasDepthStencil         = true;

  auto renderPass = m_device->createRenderPass(renderPassDesc);
  m_renderPass    = m_resourceManager->addRenderPass(std::move(renderPass), "normal_map_render_pass");
}

void NormalMapVisualizationStrategy::createFramebuffers_(const math::Dimension2i& dimension) {
  if (!m_renderPass) {
    LOG_ERROR("Render pass must be created before framebuffer");
    return;
  }

  m_framebuffers.clear();

  uint32_t framesCount = m_frameResources->getFramesCount();
  for (uint32_t i = 0; i < framesCount; i++) {
    std::string framebufferKey = "normal_map_framebuffer_" + std::to_string(i);

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

void NormalMapVisualizationStrategy::updateInstanceBuffer_(ecs::RenderModel*                    model,
                                                           const std::vector<math::Matrix4f<>>& matrices,
                                                           ModelBufferCache&                    cache) {
  if (!cache.instanceBuffer || matrices.size() > cache.capacity) {
    // If we already have a buffer, we'll let the resource manager handle freeing it

    // Create a new buffer with some growth room
    uint32_t newCapacity = std::max(static_cast<uint32_t>(matrices.size() * 1.5), 8u);

    std::string bufferKey = "normal_map_instance_buffer_" + std::to_string(reinterpret_cast<uintptr_t>(model));

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

void NormalMapVisualizationStrategy::prepareDrawCalls_(const RenderContext& context) {
  m_drawData.clear();

  for (const auto& [model, cache] : m_instanceBufferCache) {
    if (cache.count == 0) {
      continue;
    }

    for (const auto& renderMesh : model->renderMeshes) {
      rhi::DescriptorSet* materialDescriptorSet = getOrCreateMaterialDescriptorSet_(renderMesh->material);
      if (!materialDescriptorSet) {
        LOG_WARN("Could not create material descriptor set for normal map visualization");
        continue;
      }

      std::string pipelineKey
          = "normal_map_pipeline_" + std::to_string(reinterpret_cast<uintptr_t>(renderMesh->gpuMesh->vertexBuffer));

      rhi::GraphicsPipeline* pipeline = m_resourceManager->getPipeline(pipelineKey);

      if (!pipeline) {
        rhi::GraphicsPipelineDesc pipelineDesc;

        pipelineDesc.shaders.push_back(m_vertexShader);
        pipelineDesc.shaders.push_back(m_pixelShader);

        if (m_vertexShader && !m_vertexShader->getMeta().vertexInputs.empty()) {
          rhi::VertexInputBuilder::createFromReflection(m_vertexShader->getMeta().vertexInputs,
                                                        pipelineDesc.vertexBindings,
                                                        pipelineDesc.vertexAttributes,
                                                        m_device->getApiType(),
                                                        sizeof(ecs::Vertex),
                                                        sizeof(math::Matrix4f<>));

          LOG_INFO(
              "Generated vertex input from shader reflection: {} bindings, {} attributes",
              pipelineDesc.vertexBindings.size(),
              pipelineDesc.vertexAttributes.size());
        } else {
          LOG_ERROR("Shader reflection vertex inputs not available - cannot create pipeline");
          continue;
        }

        pipelineDesc.inputAssembly.topology               = rhi::PrimitiveType::Triangles;
        pipelineDesc.inputAssembly.primitiveRestartEnable = false;

        pipelineDesc.rasterization.polygonMode     = rhi::PolygonMode::Fill;
        pipelineDesc.rasterization.cullMode        = rhi::CullMode::Back;
        pipelineDesc.rasterization.frontFace       = rhi::FrontFace::Ccw;
        pipelineDesc.rasterization.depthBiasEnable = false;
        pipelineDesc.rasterization.lineWidth       = 1.0f;

        pipelineDesc.depthStencil.depthTestEnable   = true;
        pipelineDesc.depthStencil.depthWriteEnable  = true;
        pipelineDesc.depthStencil.depthCompareOp    = rhi::CompareOp::Less;
        pipelineDesc.depthStencil.stencilTestEnable = false;

        rhi::ColorBlendAttachmentDesc blendAttachment;
        blendAttachment.blendEnable    = false;
        blendAttachment.colorWriteMask = rhi::ColorMask::All;
        pipelineDesc.colorBlend.attachments.push_back(blendAttachment);

        pipelineDesc.multisample.rasterizationSamples = rhi::MSAASamples::Count1;

        std::vector<rhi::Shader*> shaders;
        if (m_vertexShader) {
          shaders.push_back(m_vertexShader);
        }
        if (m_pixelShader) {
          shaders.push_back(m_pixelShader);
        }

        rhi::PipelineLayoutDesc reflectionLayout = rhi::pipeline_utils::generatePipelineLayoutFromShaders(shaders);

        auto layoutPtrs         = m_layoutManager.createAndManageLayouts(m_device, reflectionLayout);
        pipelineDesc.setLayouts = layoutPtrs;

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

void NormalMapVisualizationStrategy::cleanupUnusedBuffers_(
    const std::unordered_map<ecs::RenderModel*, std::vector<math::Matrix4f<>>>& currentFrameInstances) {
  std::vector<ecs::RenderModel*> modelsToRemove;
  for (const auto& [model, cache] : m_instanceBufferCache) {
    if (!currentFrameInstances.contains(model)) {
      modelsToRemove.push_back(model);
    }
  }

  for (auto model : modelsToRemove) {
    m_instanceBufferCache.erase(model);
  }

  std::unordered_set<ecs::Material*> activeMaterials;

  for (const auto& [model, matrices] : currentFrameInstances) {
    for (const auto& renderMesh : model->renderMeshes) {
      if (renderMesh->material) {
        activeMaterials.insert(renderMesh->material);
      }
    }
  }

  std::vector<ecs::Material*> materialsToRemove;
  for (const auto& [material, cache] : m_materialCache) {
    if (!activeMaterials.contains(material)) {
      materialsToRemove.push_back(material);
    }
  }

  for (auto material : materialsToRemove) {
    LOG_DEBUG("Normal map visualization: Removing cached material descriptor set for deleted material at address: {}",
              reinterpret_cast<uintptr_t>(material));
    m_materialCache.erase(material);
  }
}

rhi::DescriptorSet* NormalMapVisualizationStrategy::getOrCreateMaterialDescriptorSet_(ecs::Material* material) {
  if (!material) {
    LOG_ERROR("Material is null");
    return nullptr;
  }

  auto it = m_materialCache.find(material);
  if (it != m_materialCache.end() && it->second.descriptorSet) {
    return it->second.descriptorSet;
  }

  std::string descriptorKey = "normal_map_material_" + std::to_string(reinterpret_cast<uintptr_t>(material));

  auto descriptorSetPtr = m_resourceManager->getDescriptorSet(descriptorKey);
  if (!descriptorSetPtr) {
    auto descriptorSet = m_device->createDescriptorSet(m_materialDescriptorSetLayout);

    // Update the descriptor set BEFORE adding it to the resource manager
    rhi::Texture* normalMapTexture = nullptr;
    auto          normalMapIt      = material->textures.find("normal_map");
    if (normalMapIt != material->textures.end() && normalMapIt->second) {
      normalMapTexture = normalMapIt->second;
    } else {
      normalMapTexture = m_frameResources->getDefaultNormalTexture();

      LOG_DEBUG("Using fallback normal map texture for material: {}", material->materialName);
    }

    descriptorSet->setTexture(0, normalMapTexture);

    descriptorSetPtr = m_resourceManager->addDescriptorSet(std::move(descriptorSet), descriptorKey);
  }

  m_materialCache[material].descriptorSet = descriptorSetPtr;

  return descriptorSetPtr;
}
}  // namespace renderer
}  // namespace gfx
}  // namespace arise