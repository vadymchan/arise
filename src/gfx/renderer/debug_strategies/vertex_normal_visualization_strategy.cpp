#include "gfx/renderer/debug_strategies/vertex_normal_visualization_strategy.h"

#include "ecs/components/render_model.h"
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

void VertexNormalVisualizationStrategy::initialize(rhi::Device*           device,
                                                   RenderResourceManager* resourceManager,
                                                   FrameResources*        frameResources,
                                                   rhi::ShaderManager*    shaderManager) {
  m_device          = device;
  m_resourceManager = resourceManager;
  m_frameResources  = frameResources;
  m_shaderManager   = shaderManager;

  m_vertexShader   = m_shaderManager->getShader(m_vertexShaderPath_);
  m_geometryShader = m_shaderManager->getShader(m_geometryShaderPath_);
  m_pixelShader    = m_shaderManager->getShader(m_pixelShaderPath_);

  setupRenderPass_();
}

void VertexNormalVisualizationStrategy::resize(const math::Dimension2i& newDimension) {
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

void VertexNormalVisualizationStrategy::prepareFrame(const RenderContext& context) {
  std::unordered_map<RenderModel*, std::vector<math::Matrix4f<>>> currentFrameInstances;
  std::unordered_map<RenderModel*, bool>                          modelDirtyFlags;

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

void VertexNormalVisualizationStrategy::render(const RenderContext& context) {
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

  commandBuffer->endRenderPass();
}

void VertexNormalVisualizationStrategy::clearSceneResources() {
  m_instanceBufferCache.clear();
  m_drawData.clear();
  GlobalLogger::Log(LogLevel::Info, "Vertex normal visualization strategy resources cleared for scene switch");
}

void VertexNormalVisualizationStrategy::cleanup() {
  m_instanceBufferCache.clear();
  m_drawData.clear();
  m_renderPass = nullptr;
  m_framebuffers.clear();
  m_vertexShader = nullptr;
  m_pixelShader  = nullptr;
}

void VertexNormalVisualizationStrategy::setupRenderPass_() {
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
  m_renderPass    = m_resourceManager->addRenderPass(std::move(renderPass), "normal_vis_render_pass");
}

void VertexNormalVisualizationStrategy::createFramebuffers_(const math::Dimension2i& dimension) {
  if (!m_renderPass) {
    GlobalLogger::Log(LogLevel::Error, "Render pass must be created before framebuffer");
    return;
  }

  m_framebuffers.clear();

  uint32_t framesCount = m_frameResources->getFramesCount();
  for (uint32_t i = 0; i < framesCount; i++) {
    std::string framebufferKey = "normal_vis_framebuffer_" + std::to_string(i);

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

void VertexNormalVisualizationStrategy::updateInstanceBuffer_(RenderModel*                         model,
                                                              const std::vector<math::Matrix4f<>>& matrices,
                                                              ModelBufferCache&                    cache) {
  if (!cache.instanceBuffer || matrices.size() > cache.capacity) {
    // If we already have a buffer, we'll let the resource manager handle freeing it

    // Create a new buffer with some growth room
    uint32_t newCapacity = std::max(static_cast<uint32_t>(matrices.size() * 1.5), 8u);

    std::string bufferKey = "normal_vis_instance_buffer_" + std::to_string(reinterpret_cast<uintptr_t>(model));

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

void VertexNormalVisualizationStrategy::prepareDrawCalls_(const RenderContext& context) {
  m_drawData.clear();

  auto viewLayout        = m_frameResources->getViewDescriptorSetLayout();
  auto modelMatrixLayout = m_frameResources->getModelMatrixDescriptorSetLayout();

  for (const auto& [model, cache] : m_instanceBufferCache) {
    if (cache.count == 0) {
      continue;
    }

    for (const auto& renderMesh : model->renderMeshes) {
      std::string pipelineKey
          = "normal_vis_pipeline_" + std::to_string(reinterpret_cast<uintptr_t>(renderMesh->gpuMesh->vertexBuffer));

      rhi::GraphicsPipeline* pipeline = m_resourceManager->getPipeline(pipelineKey);

      if (!pipeline) {
        rhi::GraphicsPipelineDesc pipelineDesc;

        pipelineDesc.shaders.push_back(m_vertexShader);
        pipelineDesc.shaders.push_back(m_geometryShader);
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

        rhi::VertexInputAttributeDesc normalAttr;
        normalAttr.location     = 1;
        normalAttr.binding      = 0;
        normalAttr.format       = rhi::TextureFormat::Rgb32f;
        normalAttr.offset       = offsetof(Vertex, normal);
        normalAttr.semanticName = "NORMAL";
        pipelineDesc.vertexAttributes.push_back(normalAttr);

        rhi::VertexInputAttributeDesc tangentAttr;
        tangentAttr.location     = 2;
        tangentAttr.binding      = 0;
        tangentAttr.format       = rhi::TextureFormat::Rgb32f;
        tangentAttr.offset       = offsetof(Vertex, tangent);
        tangentAttr.semanticName = "TANGENT";
        pipelineDesc.vertexAttributes.push_back(tangentAttr);

        rhi::VertexInputAttributeDesc bitangentAttr;
        bitangentAttr.location     = 3;
        bitangentAttr.binding      = 0;
        bitangentAttr.format       = rhi::TextureFormat::Rgb32f;
        bitangentAttr.offset       = offsetof(Vertex, bitangent);
        bitangentAttr.semanticName = "BITANGENT";
        pipelineDesc.vertexAttributes.push_back(bitangentAttr);

        for (uint32_t i = 0; i < 4; i++) {
          rhi::VertexInputAttributeDesc matrixCol;
          matrixCol.location     = 4 + i;
          matrixCol.binding      = 1;
          matrixCol.format       = rhi::TextureFormat::Rgba32f;
          matrixCol.offset       = i * 16;
          matrixCol.semanticName = "INSTANCE";
          pipelineDesc.vertexAttributes.push_back(matrixCol);
        }

        pipelineDesc.inputAssembly.topology               = rhi::PrimitiveType::Triangles;
        pipelineDesc.inputAssembly.primitiveRestartEnable = false;

        pipelineDesc.rasterization.polygonMode     = rhi::PolygonMode::Fill;
        pipelineDesc.rasterization.cullMode        = rhi::CullMode::None;
        pipelineDesc.rasterization.frontFace       = rhi::FrontFace::Ccw;
        pipelineDesc.rasterization.depthBiasEnable = false;
        pipelineDesc.rasterization.lineWidth       = 1.0f;

        pipelineDesc.depthStencil.depthTestEnable   = true;
        pipelineDesc.depthStencil.depthWriteEnable  = false;  // Don't write to depth
        pipelineDesc.depthStencil.depthCompareOp    = rhi::CompareOp::LessEqual;
        pipelineDesc.depthStencil.stencilTestEnable = false;

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

        pipelineDesc.setLayouts.push_back(viewLayout);
        pipelineDesc.setLayouts.push_back(modelMatrixLayout);

        pipelineDesc.renderPass = m_renderPass;

        auto pipelineObj = m_device->createGraphicsPipeline(pipelineDesc);
        pipeline         = m_resourceManager->addPipeline(std::move(pipelineObj), pipelineKey);

        m_shaderManager->registerPipelineForShader(pipeline, m_vertexShaderPath_);
        m_shaderManager->registerPipelineForShader(pipeline, m_geometryShaderPath_);
        m_shaderManager->registerPipelineForShader(pipeline, m_pixelShaderPath_);
      }

      DrawData drawData;
      drawData.pipeline                 = pipeline;
      drawData.modelMatrixDescriptorSet = m_frameResources->getOrCreateModelMatrixDescriptorSet(renderMesh);
      drawData.vertexBuffer             = renderMesh->gpuMesh->vertexBuffer;
      drawData.indexBuffer              = renderMesh->gpuMesh->indexBuffer;
      drawData.instanceBuffer           = cache.instanceBuffer;
      drawData.indexCount               = renderMesh->gpuMesh->indexBuffer->getDesc().size / sizeof(uint32_t);
      drawData.instanceCount            = cache.count;

      m_drawData.push_back(drawData);
    }
  }
}

void VertexNormalVisualizationStrategy::cleanupUnusedBuffers_(
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