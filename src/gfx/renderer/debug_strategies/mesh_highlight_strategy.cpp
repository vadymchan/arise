#include "gfx/renderer/debug_strategies/mesh_highlight_strategy.h"

#include "ecs/components/render_model.h"
#include "ecs/components/selected.h"
#include "ecs/components/vertex.h"
#include "gfx/renderer/frame_resources.h"
#include "gfx/renderer/render_resource_manager.h"
#include "gfx/rhi/interface/buffer.h"
#include "gfx/rhi/interface/descriptor.h"
#include "gfx/rhi/interface/pipeline.h"
#include "gfx/rhi/interface/render_pass.h"
#include "gfx/rhi/shader_manager.h"
#include "utils/memory/align.h"

namespace arise {
namespace gfx {
namespace renderer {

void MeshHighlightStrategy::initialize(rhi::Device*           device,
                                       RenderResourceManager* resourceManager,
                                       FrameResources*        frameResources,
                                       rhi::ShaderManager*    shaderManager) {
  m_device          = device;
  m_resourceManager = resourceManager;
  m_frameResources  = frameResources;
  m_shaderManager   = shaderManager;

  m_stencilMarkVertexShader = m_shaderManager->getShader(m_stencilMarkVertexShaderPath_);
  m_outlineVertexShader     = m_shaderManager->getShader(m_outlineVertexShaderPath_);
  m_pixelShader             = m_shaderManager->getShader(m_pixelShaderPath_);

  // Create descriptor set layout for highlight parameters
  rhi::DescriptorSetLayoutDesc        layoutDesc;
  rhi::DescriptorSetLayoutBindingDesc bindingDesc;
  bindingDesc.binding         = 0;
  bindingDesc.type            = rhi::ShaderBindingType::Uniformbuffer;
  bindingDesc.stageFlags      = rhi::ShaderStageFlag::Vertex | rhi::ShaderStageFlag::Fragment;
  bindingDesc.descriptorCount = 1;
  layoutDesc.bindings.push_back(bindingDesc);

  auto layout             = m_device->createDescriptorSetLayout(layoutDesc);
  m_highlightParamsLayout = m_resourceManager->addDescriptorSetLayout(std::move(layout), "highlight_params_layout");

  setupRenderPass_();
}

void MeshHighlightStrategy::resize(const math::Dimension2i& newDimension) {
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

void MeshHighlightStrategy::prepareFrame(const RenderContext& context) {
  std::unordered_map<RenderModel*, std::vector<math::Matrix4f<>>>     currentFrameInstances;
  std::unordered_map<RenderModel*, std::pair<math::Vector4f, float>> highlightParams;
  std::unordered_map<RenderModel*, bool>                              modelDirtyFlags;

  auto& registry = context.scene->getEntityRegistry();
  auto  view     = registry.view<Selected, RenderModel*>();

  for (auto entity : view) {
    auto& selectedComp = view.get<Selected>(entity);
    auto* renderModel  = view.get<RenderModel*>(entity);

    if (registry.all_of<Transform>(entity)) {
      auto& transform   = registry.get<Transform>(entity);
      auto  modelMatrix = calculateTransformMatrix(transform);

      currentFrameInstances[renderModel].push_back(modelMatrix);
      highlightParams[renderModel] = {selectedComp.highlightColor, selectedComp.outlineThickness};

      auto frameModels = m_frameResources->getModels();
      for (const auto& instance : frameModels) {
        if (instance->model == renderModel && instance->isDirty) {
          modelDirtyFlags[renderModel] = true;
          break;
        }
      }
    }
  }

  for (auto& [model, matrices] : currentFrameInstances) {
    auto& cache = m_instanceBufferCache[model];

    bool needsUpdate = cache.instanceBuffer == nullptr || matrices.size() > cache.capacity
                    || matrices.size() != cache.count || modelDirtyFlags[model];

    if (needsUpdate) {
      updateInstanceBuffer_(model, matrices, cache);
    }
  }

  cleanupUnusedBuffers_(currentFrameInstances);
  prepareDrawCalls_(context);
}

void MeshHighlightStrategy::render(const RenderContext& context) {
  auto commandBuffer = context.commandBuffer.get();
  if (!commandBuffer || !m_renderPass || m_framebuffers.empty() || m_drawData.empty()) {
    return;
  }

  uint32_t currentIndex = context.currentImageIndex;
  if (currentIndex >= m_framebuffers.size()) {
    GlobalLogger::Log(LogLevel::Error, "Invalid framebuffer index");
    return;
  }

  rhi::Framebuffer* currentFramebuffer = m_framebuffers[currentIndex];

  std::vector<rhi::ClearValue> clearValues;

  rhi::ClearValue colorClear = {};
  colorClear.color[0]        = 0.0f;
  colorClear.color[1]        = 0.0f;
  colorClear.color[2]        = 0.0f;
  colorClear.color[3]        = 1.0f;
  clearValues.push_back(colorClear);

  rhi::ClearValue depthStencilClear      = {};
  depthStencilClear.depthStencil.depth   = 1.0f;
  depthStencilClear.depthStencil.stencil = 0; // clear only stencil
  clearValues.push_back(depthStencilClear);

  commandBuffer->beginRenderPass(m_renderPass, currentFramebuffer, clearValues);
  commandBuffer->setViewport(m_viewport);
  commandBuffer->setScissor(m_scissor);

  // Pass 1: Stencil Mark
  for (const auto& drawData : m_drawData) {
    commandBuffer->setPipeline(drawData.stencilMarkPipeline);

    if (m_frameResources->getViewDescriptorSet()) {
      commandBuffer->bindDescriptorSet(0, m_frameResources->getViewDescriptorSet());
    }
    if (drawData.modelMatrixDescriptorSet) {
      commandBuffer->bindDescriptorSet(1, drawData.modelMatrixDescriptorSet);
    }

    commandBuffer->bindVertexBuffer(0, drawData.vertexBuffer);
    commandBuffer->bindVertexBuffer(1, drawData.instanceBuffer);
    commandBuffer->bindIndexBuffer(drawData.indexBuffer, 0, true);

    commandBuffer->drawIndexedInstanced(drawData.indexCount, drawData.instanceCount, 0, 0, 0);
  }

  // Pass 2: Outline Draw - draw outline only where stencil != 1
  for (const auto& drawData : m_drawData) {
    commandBuffer->setPipeline(drawData.outlinePipeline);

    if (m_frameResources->getViewDescriptorSet()) {
      commandBuffer->bindDescriptorSet(0, m_frameResources->getViewDescriptorSet());
    }
    if (drawData.modelMatrixDescriptorSet) {
      commandBuffer->bindDescriptorSet(1, drawData.modelMatrixDescriptorSet);
    }
    if (drawData.highlightParamsDescriptorSet) {
      commandBuffer->bindDescriptorSet(2, drawData.highlightParamsDescriptorSet);
    }

    commandBuffer->bindVertexBuffer(0, drawData.vertexBuffer);
    commandBuffer->bindVertexBuffer(1, drawData.instanceBuffer);
    commandBuffer->bindIndexBuffer(drawData.indexBuffer, 0, true);

    commandBuffer->drawIndexedInstanced(drawData.indexCount, drawData.instanceCount, 0, 0, 0);
  }

  commandBuffer->endRenderPass();
}

void MeshHighlightStrategy::clearSceneResources() {
  m_instanceBufferCache.clear();
  m_drawData.clear();
  m_pipelineCache.clear();
  m_highlightParamsCache.clear();
  GlobalLogger::Log(LogLevel::Info, "Mesh highlight strategy resources cleared for scene switch");
}

void MeshHighlightStrategy::cleanup() {
  m_instanceBufferCache.clear();
  m_drawData.clear();
  m_pipelineCache.clear();
  m_renderPass = nullptr;
  m_framebuffers.clear();
  m_stencilMarkVertexShader = nullptr;
  m_outlineVertexShader     = nullptr;
  m_pixelShader             = nullptr;
  m_highlightParamsCache.clear();
}

void MeshHighlightStrategy::setupRenderPass_() {
  rhi::RenderPassDesc renderPassDesc;

  // Color attachment
  rhi::RenderPassAttachmentDesc colorAttachmentDesc;
  colorAttachmentDesc.format        = rhi::TextureFormat::Bgra8;
  colorAttachmentDesc.samples       = rhi::MSAASamples::Count1;
  colorAttachmentDesc.loadStoreOp   = rhi::AttachmentLoadStoreOp::LoadStore;
  colorAttachmentDesc.initialLayout = rhi::ResourceLayout::ColorAttachment;
  colorAttachmentDesc.finalLayout   = rhi::ResourceLayout::ColorAttachment;
  renderPassDesc.colorAttachments.push_back(colorAttachmentDesc);

  // Depth attachment
  rhi::RenderPassAttachmentDesc depthAttachmentDesc;
  depthAttachmentDesc.format             = rhi::TextureFormat::D24S8;
  depthAttachmentDesc.samples            = rhi::MSAASamples::Count1;
  depthAttachmentDesc.loadStoreOp        = rhi::AttachmentLoadStoreOp::LoadStore;
  depthAttachmentDesc.stencilLoadStoreOp = rhi::AttachmentLoadStoreOp::ClearStore;
  depthAttachmentDesc.initialLayout      = rhi::ResourceLayout::DepthStencilAttachment;
  depthAttachmentDesc.finalLayout        = rhi::ResourceLayout::DepthStencilAttachment;
  renderPassDesc.depthStencilAttachment  = depthAttachmentDesc;
  renderPassDesc.hasDepthStencil         = true;

  auto renderPass = m_device->createRenderPass(renderPassDesc);
  m_renderPass    = m_resourceManager->addRenderPass(std::move(renderPass), "highlight_render_pass");
}

void MeshHighlightStrategy::setupVertexInput_(rhi::GraphicsPipelineDesc& pipelineDesc) {
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

  rhi::VertexInputAttributeDesc normalAttr;
  normalAttr.location     = 1;
  normalAttr.binding      = 0;
  normalAttr.format       = rhi::TextureFormat::Rgb32f;
  normalAttr.offset       = offsetof(Vertex, normal);
  normalAttr.semanticName = "NORMAL";
  pipelineDesc.vertexAttributes.push_back(normalAttr);

  for (uint32_t i = 0; i < 4; i++) {
    rhi::VertexInputAttributeDesc matrixCol;
    matrixCol.location     = 2 + i;
    matrixCol.binding      = 1;
    matrixCol.format       = rhi::TextureFormat::Rgba32f;
    matrixCol.offset       = i * 16;
    matrixCol.semanticName = "INSTANCE";
    pipelineDesc.vertexAttributes.push_back(matrixCol);
  }
}

void MeshHighlightStrategy::createFramebuffers_(const math::Dimension2i& dimension) {
  if (!m_renderPass) {
    GlobalLogger::Log(LogLevel::Error, "Render pass must be created before framebuffer");
    return;
  }

  m_framebuffers.clear();

  uint32_t framesCount = m_frameResources->getFramesCount();
  for (uint32_t i = 0; i < framesCount; i++) {
    std::string framebufferKey = "highlight_framebuffer_" + std::to_string(i);

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

void MeshHighlightStrategy::updateInstanceBuffer_(RenderModel*                         model,
                                                  const std::vector<math::Matrix4f<>>& matrices,
                                                  ModelBufferCache&                    cache) {
  if (!cache.instanceBuffer || matrices.size() > cache.capacity) {
    uint32_t newCapacity = std::max(static_cast<uint32_t>(matrices.size() * 1.5), 8u);

    std::string bufferKey = "highlight_instance_buffer_" + std::to_string(reinterpret_cast<uintptr_t>(model));

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

rhi::DescriptorSet* MeshHighlightStrategy::getOrCreateHighlightParamsDescriptorSet_(const math::Vector4f& color,
                                                                                    float                  thickness,
                                                                                    bool                   xRay) {
  uint64_t colorAsInt     = *reinterpret_cast<const uint64_t*>(&color);
  uint64_t thicknessAsInt = *reinterpret_cast<const uint64_t*>(&thickness);
  uint64_t xRayAsInt      = xRay ? 1 : 0;
  uint64_t paramsHash     = colorAsInt ^ thicknessAsInt ^ (xRayAsInt << 32);

  auto it = m_highlightParamsCache.find(paramsHash);
  if (it != m_highlightParamsCache.end()) {
    return it->second.second;
  }

  std::string bufferKey = "highlight_params_buffer_" + std::to_string(paramsHash);

  rhi::BufferDesc bufferDesc;
  bufferDesc.size        = alignConstantBufferSize(sizeof(HighlightParams));
  bufferDesc.createFlags = rhi::BufferCreateFlag::CpuAccess | rhi::BufferCreateFlag::ConstantBuffer;
  bufferDesc.type        = rhi::BufferType::Dynamic;
  bufferDesc.debugName   = bufferKey;

  auto buffer    = m_device->createBuffer(bufferDesc);
  auto bufferPtr = m_resourceManager->addBuffer(std::move(buffer), bufferKey);

  HighlightParams params;
  params.color      = color;
  params.thickness  = thickness;
  params.xRay       = xRay ? 1.0f : 0.0f;
  params.padding[0] = params.padding[1] = 0.0f;

  m_device->updateBuffer(bufferPtr, &params, sizeof(params));

  std::string descriptorKey = "highlight_params_desc_" + std::to_string(paramsHash);
  auto        descriptorSet = m_device->createDescriptorSet(m_highlightParamsLayout);
  descriptorSet->setUniformBuffer(0, bufferPtr);
  auto descriptorSetPtr = m_resourceManager->addDescriptorSet(std::move(descriptorSet), descriptorKey);

  m_highlightParamsCache[paramsHash] = {bufferPtr, descriptorSetPtr};
  return descriptorSetPtr;
}

rhi::GraphicsPipeline* MeshHighlightStrategy::getOrCreateStencilMarkPipeline_(const std::string& pipelineKey) {
  auto& cache = m_pipelineCache[pipelineKey];
  if (cache.stencilMarkPipeline) {
    return cache.stencilMarkPipeline;
  }

  std::string stencilPipelineKey = pipelineKey + "_stencil_mark";

  rhi::GraphicsPipelineDesc pipelineDesc;

  // Shaders
  pipelineDesc.shaders.push_back(m_stencilMarkVertexShader);
  pipelineDesc.shaders.push_back(m_pixelShader);

  // Vertex input
  setupVertexInput_(pipelineDesc);

  pipelineDesc.inputAssembly.topology               = rhi::PrimitiveType::Triangles;
  pipelineDesc.inputAssembly.primitiveRestartEnable = false;

  // Rasterization
  pipelineDesc.rasterization.polygonMode     = rhi::PolygonMode::Fill;
  pipelineDesc.rasterization.cullMode        = rhi::CullMode::Back;
  pipelineDesc.rasterization.frontFace       = rhi::FrontFace::Ccw;
  pipelineDesc.rasterization.depthBiasEnable = false;
  pipelineDesc.rasterization.lineWidth       = 1.0f;

  // Depth/Stencil
  pipelineDesc.depthStencil.depthTestEnable  = true;
  pipelineDesc.depthStencil.depthWriteEnable = false;
  pipelineDesc.depthStencil.depthCompareOp   = rhi::CompareOp::Always;

  // Stencil
  pipelineDesc.depthStencil.stencilTestEnable = true;
  pipelineDesc.depthStencil.front.compareOp   = rhi::CompareOp::Always;
  pipelineDesc.depthStencil.front.reference   = 1;
  pipelineDesc.depthStencil.front.passOp      = rhi::StencilOp::Replace;  // write 1 in stencil
  pipelineDesc.depthStencil.front.failOp      = rhi::StencilOp::Keep;
  pipelineDesc.depthStencil.front.depthFailOp = rhi::StencilOp::Keep;
  pipelineDesc.depthStencil.front.compareMask = 0xFF;
  pipelineDesc.depthStencil.front.writeMask   = 0xFF;
  pipelineDesc.depthStencil.back              = pipelineDesc.depthStencil.front;

  // Color blend - Not writing in color buffer
  rhi::ColorBlendAttachmentDesc blendAttachment;
  blendAttachment.blendEnable    = false;
  blendAttachment.colorWriteMask = rhi::ColorMask::None;
  pipelineDesc.colorBlend.attachments.push_back(blendAttachment);

  pipelineDesc.multisample.rasterizationSamples = rhi::MSAASamples::Count1;

  auto viewLayout        = m_frameResources->getViewDescriptorSetLayout();
  auto modelMatrixLayout = m_frameResources->getModelMatrixDescriptorSetLayout();

  pipelineDesc.setLayouts.push_back(viewLayout);
  pipelineDesc.setLayouts.push_back(modelMatrixLayout);

  pipelineDesc.renderPass = m_renderPass;

  auto pipelineObj          = m_device->createGraphicsPipeline(pipelineDesc);
  cache.stencilMarkPipeline = m_resourceManager->addPipeline(std::move(pipelineObj), stencilPipelineKey);

  m_shaderManager->registerPipelineForShader(cache.stencilMarkPipeline, m_stencilMarkVertexShaderPath_);
  m_shaderManager->registerPipelineForShader(cache.stencilMarkPipeline, m_pixelShaderPath_);

  return cache.stencilMarkPipeline;
}

rhi::GraphicsPipeline* MeshHighlightStrategy::getOrCreateOutlinePipeline_(const std::string& pipelineKey, bool xRay) {
  std::string fullPipelineKey = pipelineKey + (xRay ? "_outline_xray" : "_outline_normal");

  auto& cache = m_pipelineCache[fullPipelineKey];
  if (cache.outlinePipeline) {
    return cache.outlinePipeline;
  }

  rhi::GraphicsPipelineDesc pipelineDesc;

  pipelineDesc.shaders.push_back(m_outlineVertexShader);
  pipelineDesc.shaders.push_back(m_pixelShader);

  setupVertexInput_(pipelineDesc);

  pipelineDesc.inputAssembly.topology               = rhi::PrimitiveType::Triangles;
  pipelineDesc.inputAssembly.primitiveRestartEnable = false;

  pipelineDesc.rasterization.polygonMode = rhi::PolygonMode::Fill;
  pipelineDesc.rasterization.cullMode    = rhi::CullMode::Front;
  pipelineDesc.rasterization.frontFace   = rhi::FrontFace::Ccw;

  pipelineDesc.rasterization.depthBiasEnable         = !xRay;
  pipelineDesc.rasterization.depthBiasConstantFactor = -1.0f;
  pipelineDesc.rasterization.depthBiasSlopeFactor    = -1.0f;
  pipelineDesc.rasterization.lineWidth               = 1.0f;

  pipelineDesc.depthStencil.depthTestEnable  = true;
  pipelineDesc.depthStencil.depthWriteEnable = false;

  if (xRay) {
    pipelineDesc.depthStencil.depthCompareOp = rhi::CompareOp::Always;
  } else {
    pipelineDesc.depthStencil.depthCompareOp = rhi::CompareOp::LessEqual;
  }

  pipelineDesc.depthStencil.stencilTestEnable = true;
  pipelineDesc.depthStencil.front.compareOp   = rhi::CompareOp::NotEqual;
  pipelineDesc.depthStencil.front.reference   = 1;
  pipelineDesc.depthStencil.front.passOp      = rhi::StencilOp::Keep;
  pipelineDesc.depthStencil.front.failOp      = rhi::StencilOp::Keep;
  pipelineDesc.depthStencil.front.depthFailOp = rhi::StencilOp::Keep;
  pipelineDesc.depthStencil.front.compareMask = 0xFF;
  pipelineDesc.depthStencil.front.writeMask   = 0x00;
  pipelineDesc.depthStencil.back              = pipelineDesc.depthStencil.front;

  rhi::ColorBlendAttachmentDesc blendAttachment;
  blendAttachment.blendEnable         = true;
  blendAttachment.srcColorBlendFactor = rhi::BlendFactor::SrcAlpha;
  blendAttachment.dstColorBlendFactor = rhi::BlendFactor::OneMinusSrcAlpha;
  blendAttachment.colorBlendOp        = rhi::BlendOp::Add;
  blendAttachment.srcAlphaBlendFactor = rhi::BlendFactor::One;
  blendAttachment.dstAlphaBlendFactor = rhi::BlendFactor::Zero;
  blendAttachment.alphaBlendOp        = rhi::BlendOp::Add;
  blendAttachment.colorWriteMask      = rhi::ColorMask::All;
  pipelineDesc.colorBlend.attachments.push_back(blendAttachment);

  pipelineDesc.multisample.rasterizationSamples = rhi::MSAASamples::Count1;

  auto viewLayout        = m_frameResources->getViewDescriptorSetLayout();
  auto modelMatrixLayout = m_frameResources->getModelMatrixDescriptorSetLayout();

  pipelineDesc.setLayouts.push_back(viewLayout);
  pipelineDesc.setLayouts.push_back(modelMatrixLayout);
  pipelineDesc.setLayouts.push_back(m_highlightParamsLayout);

  pipelineDesc.renderPass = m_renderPass;

  auto pipelineObj      = m_device->createGraphicsPipeline(pipelineDesc);
  cache.outlinePipeline = m_resourceManager->addPipeline(std::move(pipelineObj), fullPipelineKey);

  m_shaderManager->registerPipelineForShader(cache.outlinePipeline, m_outlineVertexShaderPath_);
  m_shaderManager->registerPipelineForShader(cache.outlinePipeline, m_pixelShaderPath_);

  return cache.outlinePipeline;
}

void MeshHighlightStrategy::prepareDrawCalls_(const RenderContext& context) {
  m_drawData.clear();

  auto& registry = context.scene->getEntityRegistry();
  auto  view     = registry.view<Selected, RenderModel*>();

  for (auto entity : view) {
    auto& selectedComp = view.get<Selected>(entity);
    auto* renderModel  = view.get<RenderModel*>(entity);

    auto it = m_instanceBufferCache.find(renderModel);
    if (it == m_instanceBufferCache.end() || it->second.count == 0) {
      continue;
    }

    auto& cache = it->second;

    auto* highlightParamsDescriptorSet = getOrCreateHighlightParamsDescriptorSet_(
        selectedComp.highlightColor, selectedComp.outlineThickness, selectedComp.xRay);

    for (const auto& renderMesh : renderModel->renderMeshes) {
      std::string pipelineKey
          = "highlight_pipeline_" + std::to_string(reinterpret_cast<uintptr_t>(renderMesh->gpuMesh->vertexBuffer));

      rhi::GraphicsPipeline* stencilMarkPipeline = getOrCreateStencilMarkPipeline_(pipelineKey);
      rhi::GraphicsPipeline* outlinePipeline     = getOrCreateOutlinePipeline_(pipelineKey, selectedComp.xRay);

      if (!stencilMarkPipeline || !outlinePipeline) {
        GlobalLogger::Log(LogLevel::Error, "Failed to create highlight pipelines");
        continue;
      }

      DrawData drawData;
      drawData.stencilMarkPipeline          = stencilMarkPipeline;
      drawData.outlinePipeline              = outlinePipeline;
      drawData.modelMatrixDescriptorSet     = m_frameResources->getOrCreateModelMatrixDescriptorSet(renderMesh);
      drawData.highlightParamsDescriptorSet = highlightParamsDescriptorSet;
      drawData.vertexBuffer                 = renderMesh->gpuMesh->vertexBuffer;
      drawData.indexBuffer                  = renderMesh->gpuMesh->indexBuffer;
      drawData.instanceBuffer               = cache.instanceBuffer;
      drawData.indexCount                   = renderMesh->gpuMesh->indexBuffer->getDesc().size / sizeof(uint32_t);
      drawData.instanceCount                = cache.count;

      m_drawData.push_back(drawData);
    }
  }
}

void MeshHighlightStrategy::cleanupUnusedBuffers_(
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
}

}  // namespace renderer
}  // namespace gfx
}  // namespace arise