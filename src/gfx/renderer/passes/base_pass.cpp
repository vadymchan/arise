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
#include "gfx/rhi/shader_reflection/pipeline_layout_builder.h"
#include "gfx/rhi/shader_reflection/pipeline_layout_manager.h"
#include "gfx/rhi/shader_reflection/pipeline_utils.h"
#include "gfx/rhi/shader_reflection/shader_reflection_utils.h"
#include "gfx/rhi/shader_reflection/vertex_input_builder.h"
#include "profiler/profiler.h"
#include "utils/logger/log.h"

#include <algorithm>
#include <cstring>

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
    LOG_ERROR("ShaderManager not found");
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

  std::unordered_map<ecs::RenderModel*, std::vector<math::Matrix4f<>>> currentFrameInstances;
  std::unordered_map<ecs::RenderModel*, bool>                          modelDirtyFlags;

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
    LOG_ERROR("Invalid framebuffer index");
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
  LOG_INFO("Base pass resources cleared for scene switch");
}

void BasePass::cleanup() {
  clearSceneResources();

  m_framebuffers.clear();

  m_renderPass = nullptr;

  m_vertexShader = nullptr;
  m_pixelShader  = nullptr;

  m_layoutManager.cleanup();
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
    LOG_ERROR("Render pass must be created before framebuffer");
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

void BasePass::updateInstanceBuffer_(ecs::RenderModel*                    model,
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

  for (const auto& [model, cache] : m_instanceBufferCache) {
    if (cache.count == 0) {
      continue;
    }

    for (const auto& renderMesh : model->renderMeshes) {
      if (!renderMesh->material) {
        LOG_DEBUG("RenderMesh has null material, skipping");
        continue;
      }

      rhi::DescriptorSet* materialDescriptorSet = getOrCreateMaterialDescriptorSet_(renderMesh->material);

      if (!materialDescriptorSet) {
        LOG_DEBUG("Could not create valid material descriptor set, skipping");
        continue;
      }

      std::string pipelineKey
          = "base_pipeline_" + std::to_string(reinterpret_cast<uintptr_t>(renderMesh->gpuMesh->vertexBuffer));

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

          LOG_INFO("Generated vertex input from shader reflection: {} bindings, {} attributes",
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

void BasePass::cleanupUnusedBuffers_(
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
    LOG_DEBUG("Removing cached material parameters for deleted material at address: {}",
              reinterpret_cast<uintptr_t>(material));
    m_materialCache.erase(material);
  }
}

rhi::DescriptorSet* BasePass::getOrCreateMaterialDescriptorSet_(ecs::Material* material) {
  if (!material) {
    LOG_ERROR("Material is null");
    return nullptr;
  }

  auto it = m_materialCache.find(material);
  if (it != m_materialCache.end() && it->second.descriptorSet) {
    return it->second.descriptorSet;
  }

  auto materialLayout = m_resourceManager->getDescriptorSetLayout("material_layout");
  if (!materialLayout) {
    LOG_ERROR("Material descriptor set layout not found");
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

        LOG_DEBUG("Using fallback texture for '{}' in material: {}", textureName, material->materialName);
      }

      if (!texture) {
        LOG_ERROR(
            "No texture available (including fallback) for '{}' in material: {}", textureName, material->materialName);
        allTexturesValid = false;
        break;
      }

      descriptorSet->setTexture(binding, texture);
      binding++;
    }

    if (allTexturesValid) {
      descriptorSetPtr = m_resourceManager->addDescriptorSet(std::move(descriptorSet), materialKey);
    } else {
      LOG_WARN("Material descriptor set creation failed due to invalid textures: {}", material->materialName);
      return nullptr;
    }
  }

  m_materialCache[material].descriptorSet = descriptorSetPtr;
  return descriptorSetPtr;
}
}  // namespace renderer
}  // namespace gfx
}  // namespace arise
