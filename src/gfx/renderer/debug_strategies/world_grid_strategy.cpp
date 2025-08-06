#include "gfx/renderer/debug_strategies/world_grid_strategy.h"

#include "ecs/components/camera.h"
#include "gfx/renderer/frame_resources.h"
#include "gfx/renderer/render_resource_manager.h"
#include "gfx/rhi/interface/buffer.h"
#include "gfx/rhi/interface/descriptor.h"
#include "gfx/rhi/interface/pipeline.h"
#include "gfx/rhi/shader_manager.h"
#include "utils/memory/align.h"

namespace arise {
namespace gfx {
namespace renderer {

void WorldGridStrategy::initialize(rhi::Device*           device,
                                   RenderResourceManager* resourceManager,
                                   FrameResources*        frameResources,
                                   rhi::ShaderManager*    shaderManager) {
  m_device          = device;
  m_resourceManager = resourceManager;
  m_frameResources  = frameResources;
  m_shaderManager   = shaderManager;

  m_vertexShader = m_shaderManager->getShader(m_vertexShaderPath_);
  m_pixelShader  = m_shaderManager->getShader(m_pixelShaderPath_);

  setupRenderPass_();
  createPipeline_();
}

void WorldGridStrategy::resize(const math::Dimension2i& newDimension) {
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

void WorldGridStrategy::prepareFrame(const RenderContext& context) {
  // updateGridParameters_(context);
}

void WorldGridStrategy::render(const RenderContext& context) {
  auto commandBuffer = context.commandBuffer.get();
  if (!commandBuffer || !m_renderPass || m_framebuffers.empty() || !m_pipeline) {
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

  commandBuffer->setPipeline(m_pipeline);

  if (m_frameResources->getViewDescriptorSet()) {
    commandBuffer->bindDescriptorSet(0, m_frameResources->getViewDescriptorSet());
  }

  // commandBuffer->bindDescriptorSet(0, m_gridDescriptorSet);

  // Draw fullscreen triangle
  commandBuffer->draw(3, 0);

  commandBuffer->endRenderPass();
}

void WorldGridStrategy::clearSceneResources() {
  LOG_WARN(
      "World grid strategy tried to clear resources after scene switch (it doesn't have anything dependent on that)");
}

void WorldGridStrategy::cleanup() {
  // m_gridDescriptorSet    = nullptr;
  // m_gridLayout           = nullptr;
  // m_gridParametersBuffer = nullptr;
  m_pipeline   = nullptr;
  m_renderPass = nullptr;
  m_framebuffers.clear();
  m_vertexShader = nullptr;
  m_pixelShader  = nullptr;
}

void WorldGridStrategy::setupRenderPass_() {
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
  m_renderPass    = m_resourceManager->addRenderPass(std::move(renderPass), "world_grid_render_pass");
}

void WorldGridStrategy::createFramebuffers_(const math::Dimension2i& dimension) {
  if (!m_renderPass) {
    LOG_ERROR("Render pass must be created before framebuffer");
    return;
  }

  m_framebuffers.clear();

  uint32_t framesCount = m_frameResources->getFramesCount();
  for (uint32_t i = 0; i < framesCount; i++) {
    std::string framebufferKey = "world_grid_framebuffer_" + std::to_string(i);

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

void WorldGridStrategy::createPipeline_() {
  rhi::GraphicsPipelineDesc pipelineDesc;

  pipelineDesc.shaders.push_back(m_vertexShader);
  pipelineDesc.shaders.push_back(m_pixelShader);

  pipelineDesc.inputAssembly.topology               = rhi::PrimitiveType::Triangles;
  pipelineDesc.inputAssembly.primitiveRestartEnable = false;

  pipelineDesc.rasterization.polygonMode     = rhi::PolygonMode::Fill;
  pipelineDesc.rasterization.cullMode        = rhi::CullMode::None;
  pipelineDesc.rasterization.frontFace       = rhi::FrontFace::Ccw;
  pipelineDesc.rasterization.depthBiasEnable = false;
  pipelineDesc.rasterization.lineWidth       = 1.0f;

  pipelineDesc.depthStencil.depthTestEnable   = true;
  pipelineDesc.depthStencil.depthWriteEnable  = true;
  pipelineDesc.depthStencil.depthCompareOp    = rhi::CompareOp::Less;
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

  pipelineDesc.setLayouts.push_back(m_frameResources->getViewDescriptorSetLayout());

  pipelineDesc.renderPass = m_renderPass;

  auto pipelineObj = m_device->createGraphicsPipeline(pipelineDesc);
  m_pipeline       = m_resourceManager->addPipeline(std::move(pipelineObj), "world_grid_pipeline");

  m_shaderManager->registerPipelineForShader(m_pipeline, m_vertexShaderPath_);
  m_shaderManager->registerPipelineForShader(m_pipeline, m_pixelShaderPath_);
}

}  // namespace renderer
}  // namespace gfx
}  // namespace arise