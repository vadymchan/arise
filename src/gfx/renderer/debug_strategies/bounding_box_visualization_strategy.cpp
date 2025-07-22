#include "gfx/renderer/debug_strategies/bounding_box_visualization_strategy.h"

#include "ecs/components/bounding_volume.h"
#include "ecs/components/model.h"
#include "ecs/components/render_model.h"
#include "ecs/components/transform.h"
#include "ecs/components/vertex.h"
#include "gfx/renderer/frame_resources.h"
#include "gfx/renderer/render_resource_manager.h"
#include "gfx/rhi/interface/buffer.h"
#include "gfx/rhi/interface/descriptor.h"
#include "gfx/rhi/interface/pipeline.h"
#include "gfx/rhi/interface/render_pass.h"
#include "gfx/rhi/shader_manager.h"
#include "profiler/profiler.h"
#include "utils/logger/global_logger.h"

namespace arise {
namespace gfx {
namespace renderer {

void BoundingBoxVisualizationStrategy::initialize(rhi::Device*           device,
                                                  RenderResourceManager* resourceManager,
                                                  FrameResources*        frameResources,
                                                  rhi::ShaderManager*    shaderManager) {
  m_device          = device;
  m_resourceManager = resourceManager;
  m_frameResources  = frameResources;
  m_shaderManager   = shaderManager;

  m_vertexShader = m_shaderManager->getShader(m_vertexShaderPath_);
  m_pixelShader  = m_shaderManager->getShader(m_pixelShaderPath_);

  createBoundingBoxGeometry_();
  setupRenderPass_();
}

void BoundingBoxVisualizationStrategy::resize(const math::Dimension2i& newDimension) {
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

void BoundingBoxVisualizationStrategy::prepareFrame(const RenderContext& context) {
  std::unordered_map<RenderModel*, std::vector<BoundingBoxData>> currentFrameInstances;
  std::unordered_map<RenderModel*, bool>                         modelDirtyFlags;

  auto& registry = context.scene->getEntityRegistry();

  for (const auto& instance : m_frameResources->getModels()) {
    if (registry.all_of<Model*, WorldBounds>(instance->entityId)) {
      auto* model       = registry.get<Model*>(instance->entityId);
      auto& worldBounds = registry.get<WorldBounds>(instance->entityId);

      if (!model || !bounds::isValid(model->boundingBox)) {
        continue;
      }

      BoundingBoxData boundingBoxData;

      // Local bounds + transform mode (OBB)
      boundingBoxData.modelMatrix    = instance->modelMatrix;
      boundingBoxData.localMinCorner = model->boundingBox.min;
      boundingBoxData.localMaxCorner = model->boundingBox.max;

      // World bounds mode (AABB)
      if (bounds::isValid(worldBounds.boundingBox)) {
        boundingBoxData.worldMinCorner = worldBounds.boundingBox.min;
        boundingBoxData.worldMaxCorner = worldBounds.boundingBox.max;
      } else {
        GlobalLogger::Log(LogLevel::Error, "World bounds are invalid");
        boundingBoxData.worldMinCorner = model->boundingBox.min;
        boundingBoxData.worldMaxCorner = model->boundingBox.max;
      }

      currentFrameInstances[instance->model].push_back(boundingBoxData);

      if ((worldBounds.isDirty || instance->isDirty) && !modelDirtyFlags[instance->model]) {
        modelDirtyFlags[instance->model] = true;
      }
    }
  }

  for (auto& [model, boundingBoxDataList] : currentFrameInstances) {
    auto& cache = m_instanceBufferCache[model];

    bool needsUpdate = cache.instanceBuffer == nullptr ||              // Buffer not created yet
                       boundingBoxDataList.size() > cache.capacity ||  // Need more space
                       boundingBoxDataList.size() != cache.count ||    // Count changed
                       modelDirtyFlags[model];                         // Model was modified

    if (needsUpdate) {
      updateInstanceBuffer_(model, boundingBoxDataList, cache);
    }
  }

  cleanupUnusedBuffers_(currentFrameInstances);
  prepareDrawCalls_(context);
}

void BoundingBoxVisualizationStrategy::render(const RenderContext& context) {
  CPU_ZONE_NC("BoundingBox Strategy", color::ORANGE);

  auto commandBuffer = context.commandBuffer.get();
  if (!commandBuffer || !m_renderPass || m_framebuffers.empty()) {
    return;
  }

  uint32_t currentIndex = context.currentImageIndex;
  if (currentIndex >= m_framebuffers.size()) {
    GlobalLogger::Log(LogLevel::Error, "Invalid framebuffer index");
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

  {
    CPU_ZONE_NC("Draw Bounding Boxes", color::GREEN);
    for (const auto& drawData : m_drawData) {
      commandBuffer->setPipeline(drawData.pipeline);

      if (m_frameResources->getViewDescriptorSet()) {
        commandBuffer->bindDescriptorSet(0, m_frameResources->getViewDescriptorSet());
      }

      if (drawData.modelMatrixDescriptorSet) {
        commandBuffer->bindDescriptorSet(1, drawData.modelMatrixDescriptorSet);
      }

      commandBuffer->bindVertexBuffer(0, m_cubeVertexBuffer);
      commandBuffer->bindVertexBuffer(1, drawData.instanceBuffer);
      commandBuffer->bindIndexBuffer(m_cubeIndexBuffer, 0, true);

      constexpr uint32_t cubeIndicesCount = 24;
      commandBuffer->drawIndexedInstanced(cubeIndicesCount, drawData.instanceCount, 0, 0, 0);
    }
  }

  commandBuffer->endRenderPass();
}

void BoundingBoxVisualizationStrategy::clearSceneResources() {
  m_instanceBufferCache.clear();
  m_drawData.clear();
  GlobalLogger::Log(LogLevel::Info, "Bounding box visualization strategy resources cleared for scene switch");
}

void BoundingBoxVisualizationStrategy::cleanup() {
  m_instanceBufferCache.clear();
  m_drawData.clear();

  m_cubeVertexBuffer = nullptr;
  m_cubeIndexBuffer  = nullptr;

  m_renderPass = nullptr;
  m_framebuffers.clear();
  m_vertexShader = nullptr;
  m_pixelShader  = nullptr;
}

void BoundingBoxVisualizationStrategy::setupRenderPass_() {
  rhi::RenderPassDesc renderPassDesc;

  rhi::RenderPassAttachmentDesc colorAttachmentDesc;
  colorAttachmentDesc.format        = rhi::TextureFormat::Bgra8;
  colorAttachmentDesc.samples       = rhi::MSAASamples::Count1;
  colorAttachmentDesc.loadStoreOp   = rhi::AttachmentLoadStoreOp::LoadStore;
  colorAttachmentDesc.initialLayout = rhi::ResourceLayout::ColorAttachment;
  colorAttachmentDesc.finalLayout   = rhi::ResourceLayout::ColorAttachment;
  renderPassDesc.colorAttachments.push_back(colorAttachmentDesc);

  rhi::RenderPassAttachmentDesc depthAttachmentDesc;
  depthAttachmentDesc.format             = rhi::TextureFormat::D24S8;
  depthAttachmentDesc.samples            = rhi::MSAASamples::Count1;
  depthAttachmentDesc.loadStoreOp        = rhi::AttachmentLoadStoreOp::LoadStore;
  depthAttachmentDesc.stencilLoadStoreOp = rhi::AttachmentLoadStoreOp::DontcareDontcare;
  depthAttachmentDesc.initialLayout      = rhi::ResourceLayout::DepthStencilAttachment;
  depthAttachmentDesc.finalLayout        = rhi::ResourceLayout::DepthStencilAttachment;
  renderPassDesc.depthStencilAttachment  = depthAttachmentDesc;
  renderPassDesc.hasDepthStencil         = true;

  auto renderPass = m_device->createRenderPass(renderPassDesc);
  m_renderPass    = m_resourceManager->addRenderPass(std::move(renderPass), "bounding_box_render_pass");
}

void BoundingBoxVisualizationStrategy::createFramebuffers_(const math::Dimension2i& dimension) {
  if (!m_renderPass) {
    GlobalLogger::Log(LogLevel::Error, "Render pass must be created before framebuffer");
    return;
  }

  m_framebuffers.clear();

  uint32_t framesCount = m_frameResources->getFramesCount();
  for (uint32_t i = 0; i < framesCount; i++) {
    std::string framebufferKey = "bounding_box_framebuffer_" + std::to_string(i);

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

void BoundingBoxVisualizationStrategy::prepareDrawCalls_(const RenderContext& context) {
  m_drawData.clear();

  auto viewDescriptorSetLayout = m_frameResources->getViewDescriptorSetLayout();
  auto modelMatrixLayout       = m_frameResources->getModelMatrixDescriptorSetLayout();

  for (const auto& [model, cache] : m_instanceBufferCache) {
    if (cache.count == 0) {
      continue;
    }

    std::string pipelineKey = "bounding_box_pipeline_" + std::to_string(reinterpret_cast<uintptr_t>(model));

    rhi::GraphicsPipeline* pipeline = m_resourceManager->getPipeline(pipelineKey);

    if (!pipeline) {
      rhi::GraphicsPipelineDesc pipelineDesc;

      pipelineDesc.shaders.push_back(m_vertexShader);
      pipelineDesc.shaders.push_back(m_pixelShader);

      rhi::VertexInputBindingDesc vertexBinding;
      vertexBinding.binding   = 0;
      vertexBinding.stride    = sizeof(math::Vector3f);
      vertexBinding.inputRate = rhi::VertexInputRate::Vertex;
      pipelineDesc.vertexBindings.push_back(vertexBinding);

      rhi::VertexInputBindingDesc instanceBinding;
      instanceBinding.binding   = 1;
      instanceBinding.stride    = sizeof(BoundingBoxData);
      instanceBinding.inputRate = rhi::VertexInputRate::Instance;
      pipelineDesc.vertexBindings.push_back(instanceBinding);

      rhi::VertexInputAttributeDesc positionAttr;
      positionAttr.location     = 0;
      positionAttr.binding      = 0;
      positionAttr.format       = rhi::TextureFormat::Rgb32f;
      positionAttr.offset       = 0;
      positionAttr.semanticName = "POSITION";
      pipelineDesc.vertexAttributes.push_back(positionAttr);

      for (int i = 0; i < 4; ++i) {
        rhi::VertexInputAttributeDesc matrixAttr;
        matrixAttr.location     = 1 + i;
        matrixAttr.binding      = 1;
        matrixAttr.format       = rhi::TextureFormat::Rgba32f;
        matrixAttr.offset       = offsetof(BoundingBoxData, modelMatrix) + i * sizeof(math::Vector4f);
        matrixAttr.semanticName = "INSTANCE";
        pipelineDesc.vertexAttributes.push_back(matrixAttr);
      }

      rhi::VertexInputAttributeDesc localMinCornerAttr;
      localMinCornerAttr.location     = 5;
      localMinCornerAttr.binding      = 1;
      localMinCornerAttr.format       = rhi::TextureFormat::Rgb32f;
      localMinCornerAttr.offset       = offsetof(BoundingBoxData, localMinCorner);
      localMinCornerAttr.semanticName = "LOCAL_MIN_CORNER";
      pipelineDesc.vertexAttributes.push_back(localMinCornerAttr);

      rhi::VertexInputAttributeDesc localMaxCornerAttr;
      localMaxCornerAttr.location     = 6;
      localMaxCornerAttr.binding      = 1;
      localMaxCornerAttr.format       = rhi::TextureFormat::Rgb32f;
      localMaxCornerAttr.offset       = offsetof(BoundingBoxData, localMaxCorner);
      localMaxCornerAttr.semanticName = "LOCAL_MAX_CORNER";
      pipelineDesc.vertexAttributes.push_back(localMaxCornerAttr);

      rhi::VertexInputAttributeDesc worldMinCornerAttr;
      worldMinCornerAttr.location     = 7;
      worldMinCornerAttr.binding      = 1;
      worldMinCornerAttr.format       = rhi::TextureFormat::Rgb32f;
      worldMinCornerAttr.offset       = offsetof(BoundingBoxData, worldMinCorner);
      worldMinCornerAttr.semanticName = "WORLD_MIN_CORNER";
      pipelineDesc.vertexAttributes.push_back(worldMinCornerAttr);

      rhi::VertexInputAttributeDesc worldMaxCornerAttr;
      worldMaxCornerAttr.location     = 8;
      worldMaxCornerAttr.binding      = 1;
      worldMaxCornerAttr.format       = rhi::TextureFormat::Rgb32f;
      worldMaxCornerAttr.offset       = offsetof(BoundingBoxData, worldMaxCorner);
      worldMaxCornerAttr.semanticName = "WORLD_MAX_CORNER";
      pipelineDesc.vertexAttributes.push_back(worldMaxCornerAttr);

      pipelineDesc.inputAssembly.topology               = rhi::PrimitiveType::Lines;
      pipelineDesc.inputAssembly.primitiveRestartEnable = false;

      pipelineDesc.rasterization.polygonMode = rhi::PolygonMode::Line;
      pipelineDesc.rasterization.cullMode    = rhi::CullMode::None;
      pipelineDesc.rasterization.frontFace   = rhi::FrontFace::Ccw;
      pipelineDesc.rasterization.lineWidth   = 1.0f;

      pipelineDesc.depthStencil.depthTestEnable  = true;
      pipelineDesc.depthStencil.depthWriteEnable = false;
      pipelineDesc.depthStencil.depthCompareOp   = rhi::CompareOp::LessEqual;

      rhi::ColorBlendAttachmentDesc blendAttachment;
      blendAttachment.blendEnable    = false;
      blendAttachment.colorWriteMask = rhi::ColorMask::All;
      pipelineDesc.colorBlend.attachments.push_back(blendAttachment);

      pipelineDesc.multisample.rasterizationSamples = rhi::MSAASamples::Count1;

      pipelineDesc.setLayouts.push_back(viewDescriptorSetLayout);
      pipelineDesc.setLayouts.push_back(modelMatrixLayout);

      pipelineDesc.renderPass = m_renderPass;

      auto pipelineObj = m_device->createGraphicsPipeline(pipelineDesc);
      pipeline         = m_resourceManager->addPipeline(std::move(pipelineObj), pipelineKey);

      m_shaderManager->registerPipelineForShader(pipeline, m_vertexShaderPath_);
      m_shaderManager->registerPipelineForShader(pipeline, m_pixelShaderPath_);
    }

    if (!pipeline) {
      GlobalLogger::Log(LogLevel::Error, "Failed to create bounding box pipeline");
      continue;
    }

    if (!model->renderMeshes.empty()) {
      auto renderMesh = model->renderMeshes[0];

      DrawData drawData;
      drawData.pipeline                 = pipeline;
      drawData.modelMatrixDescriptorSet = m_frameResources->getOrCreateModelMatrixDescriptorSet(renderMesh);
      drawData.instanceBuffer           = cache.instanceBuffer;
      drawData.instanceCount            = cache.count;

      m_drawData.push_back(drawData);
    }
  }
}

void BoundingBoxVisualizationStrategy::updateInstanceBuffer_(RenderModel*                        model,
                                                             const std::vector<BoundingBoxData>& boundingBoxData,
                                                             ModelBufferCache&                   cache) {
  if (!cache.instanceBuffer || boundingBoxData.size() > cache.capacity) {

    uint32_t newCapacity = std::max(static_cast<uint32_t>(boundingBoxData.size() * 1.5), 8u);

    std::string bufferKey = "bounding_box_instance_buffer_" + std::to_string(reinterpret_cast<uintptr_t>(model));

    rhi::BufferDesc bufferDesc;
    bufferDesc.size        = newCapacity * sizeof(BoundingBoxData);
    bufferDesc.createFlags = rhi::BufferCreateFlag::InstanceBuffer;
    bufferDesc.type        = rhi::BufferType::Dynamic;
    bufferDesc.stride      = sizeof(BoundingBoxData);
    bufferDesc.debugName   = bufferKey;

    auto buffer          = m_device->createBuffer(bufferDesc);
    cache.instanceBuffer = m_resourceManager->addBuffer(std::move(buffer), bufferKey);
    cache.capacity       = newCapacity;
  }

  if (cache.instanceBuffer && !boundingBoxData.empty()) {
    m_device->updateBuffer(
        cache.instanceBuffer, boundingBoxData.data(), boundingBoxData.size() * sizeof(BoundingBoxData));
  }

  cache.count = static_cast<uint32_t>(boundingBoxData.size());
}

void BoundingBoxVisualizationStrategy::cleanupUnusedBuffers_(
    const std::unordered_map<RenderModel*, std::vector<BoundingBoxData>>& currentFrameInstances) {
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

void BoundingBoxVisualizationStrategy::createBoundingBoxGeometry_() {

  std::vector<math::Vector3f> cubeVertices = {
  // Bottom face
    {-0.5f, -0.5f, -0.5f},
    { 0.5f, -0.5f, -0.5f},
    { 0.5f, -0.5f,  0.5f},
    {-0.5f, -0.5f,  0.5f},
 // Top face
    {-0.5f,  0.5f, -0.5f},
    { 0.5f,  0.5f, -0.5f},
    { 0.5f,  0.5f,  0.5f},
    {-0.5f,  0.5f,  0.5f}
  };

  // clang-format off
  std::vector<uint32_t> cubeIndices = {
      // Bottom face
      0, 1, 1, 2, 2, 3, 3, 0,
      // Top face
      4, 5, 5, 6, 6, 7, 7, 4,
      // Vertical edges
      0, 4, 1, 5, 2, 6, 3, 7};
  // clang-format on

  rhi::BufferDesc vertexBufferDesc;
  vertexBufferDesc.size        = cubeVertices.size() * sizeof(math::Vector3f);
  vertexBufferDesc.createFlags = rhi::BufferCreateFlag::VertexBuffer;
  vertexBufferDesc.type        = rhi::BufferType::Static;
  vertexBufferDesc.stride      = sizeof(math::Vector3f);
  vertexBufferDesc.debugName   = "bounding_box_vertex_buffer";

  auto vertexBuffer  = m_device->createBuffer(vertexBufferDesc);
  m_cubeVertexBuffer = m_resourceManager->addBuffer(std::move(vertexBuffer), "bounding_box_vertex_buffer");

  if (m_cubeVertexBuffer) {
    m_device->updateBuffer(m_cubeVertexBuffer, cubeVertices.data(), cubeVertices.size() * sizeof(math::Vector3f));
  }

  rhi::BufferDesc indexBufferDesc;
  indexBufferDesc.size        = cubeIndices.size() * sizeof(uint32_t);
  indexBufferDesc.createFlags = rhi::BufferCreateFlag::IndexBuffer;
  indexBufferDesc.type        = rhi::BufferType::Static;
  indexBufferDesc.stride      = sizeof(uint32_t);
  indexBufferDesc.debugName   = "bounding_box_index_buffer";

  auto indexBuffer  = m_device->createBuffer(indexBufferDesc);
  m_cubeIndexBuffer = m_resourceManager->addBuffer(std::move(indexBuffer), "bounding_box_index_buffer");

  if (m_cubeIndexBuffer) {
    m_device->updateBuffer(m_cubeIndexBuffer, cubeIndices.data(), cubeIndices.size() * sizeof(uint32_t));
  }

  if (!m_cubeVertexBuffer || !m_cubeIndexBuffer) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create bounding box geometry buffers");
  }
}

}  // namespace renderer
}  // namespace gfx
}  // namespace arise