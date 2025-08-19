#include "gfx/rhi/backends/vulkan/pipeline_vk.h"

#include "gfx/rhi/backends/vulkan/descriptor_vk.h"
#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "gfx/rhi/backends/vulkan/render_pass_vk.h"
#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"
#include "gfx/rhi/backends/vulkan/shader_vk.h"
#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

GraphicsPipelineVk::GraphicsPipelineVk(const GraphicsPipelineDesc& desc,
                                       const PipelineLayoutDesc&   reflectionLayout,
                                       DeviceVk*                   device)
    : GraphicsPipeline(desc, reflectionLayout)
    , m_device_(device)
    , m_pipeline_(VK_NULL_HANDLE)
    , m_pipelineLayout_(VK_NULL_HANDLE) {
  m_ownedLayouts_.reserve(desc.setLayouts.size());
  std::vector<const DescriptorSetLayout*> ownedLayoutPtrs;
  ownedLayoutPtrs.reserve(desc.setLayouts.size());

  for (const auto* layout : desc.setLayouts) {
    if (layout) {
      auto copiedLayout = m_device_->createDescriptorSetLayout(layout->getDesc());
      ownedLayoutPtrs.push_back(copiedLayout.get());
      m_ownedLayouts_.push_back(std::move(copiedLayout));
    } else {
      ownedLayoutPtrs.push_back(nullptr);
    }
  }

  m_desc_.setLayouts = ownedLayoutPtrs;

  if (!initialize_()) {
    LOG_ERROR("Failed to initialize Vulkan graphics pipeline");
  }
}

GraphicsPipelineVk::~GraphicsPipelineVk() {
  if (m_pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_device_->getDevice(), m_pipeline_, nullptr);
    m_pipeline_ = VK_NULL_HANDLE;
  }

  if (m_pipelineLayout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_device_->getDevice(), m_pipelineLayout_, nullptr);
    m_pipelineLayout_ = VK_NULL_HANDLE;
  }
}

bool GraphicsPipelineVk::rebuild() {
  if (createPipeline_()) {
    LOG_INFO("Successfully rebuilt Vulkan graphics pipeline");
    m_updateFrame = -1;
    return true;
  }

  LOG_ERROR("Failed to rebuild Vulkan graphics pipeline");
  return false;
}

bool GraphicsPipelineVk::initialize_() {
  if (!createPipelineLayout_()) {
    LOG_ERROR("Failed to create pipeline layout");
    return false;
  }

  return createPipeline_();
}

bool GraphicsPipelineVk::createPipeline_() {
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
  if (!createShaderStages_(shaderStages)) {
    LOG_ERROR("Failed to create shader stages");
    return false;
  }

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  if (!createVertexInputState_(vertexInputInfo)) {
    LOG_ERROR("Failed to create vertex input state");
    return false;
  }

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  if (!createInputAssemblyState_(inputAssembly)) {
    LOG_ERROR("Failed to create input assembly state");
    return false;
  }

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount                     = 1;
  viewportState.scissorCount                      = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  if (!createRasterizationState_(rasterizer)) {
    LOG_ERROR("Failed to create rasterization state");
    return false;
  }

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  if (!createMultisampleState_(multisampling)) {
    LOG_ERROR("Failed to create multisample state");
    return false;
  }

  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  if (!createDepthStencilState_(depthStencil)) {
    LOG_ERROR("Failed to create depth-stencil state");
    return false;
  }

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  if (!createColorBlendState_(colorBlending)) {
    LOG_ERROR("Failed to create color blend state");
    return false;
  }

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType                            = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount                = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates                   = dynamicStates.data();

  RenderPassVk* renderPassVk = dynamic_cast<RenderPassVk*>(m_desc_.renderPass);
  if (!renderPassVk) {
    LOG_ERROR("Invalid render pass for Vulkan graphics pipeline");
    return false;
  }

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount                   = static_cast<uint32_t>(shaderStages.size());
  pipelineInfo.pStages                      = shaderStages.data();
  pipelineInfo.pVertexInputState            = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState          = &inputAssembly;
  pipelineInfo.pViewportState               = &viewportState;
  pipelineInfo.pRasterizationState          = &rasterizer;
  pipelineInfo.pMultisampleState            = &multisampling;
  pipelineInfo.pDepthStencilState           = &depthStencil;
  pipelineInfo.pColorBlendState             = &colorBlending;
  pipelineInfo.pDynamicState                = &dynamicState;
  pipelineInfo.layout                       = m_pipelineLayout_;
  pipelineInfo.renderPass                   = renderPassVk->getRenderPass();
  pipelineInfo.subpass                      = m_desc_.subpass;
  pipelineInfo.basePipelineHandle           = VK_NULL_HANDLE;
  pipelineInfo.basePipelineIndex            = -1;

  // Destroy existing pipeline if it exists
  if (m_pipeline_ != VK_NULL_HANDLE) {
    // TODO: temp, dirty fix. Consider add delayed destroy for old pipeline
    m_device_->waitIdle();
    vkDestroyPipeline(m_device_->getDevice(), m_pipeline_, nullptr);
    m_pipeline_ = VK_NULL_HANDLE;
  }

  if (vkCreateGraphicsPipelines(m_device_->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline_)
      != VK_SUCCESS) {
    LOG_ERROR("Failed to create graphics pipeline");
    return false;
  }

  return true;
}

bool GraphicsPipelineVk::createShaderStages_(std::vector<VkPipelineShaderStageCreateInfo>& shaderStages) {
  shaderStages.clear();

  for (Shader* shader : m_desc_.shaders) {
    if (!shader) {
      LOG_ERROR("Null shader pointer in pipeline description");
      return false;
    }

    ShaderVk* shaderVk = dynamic_cast<ShaderVk*>(shader);
    if (!shaderVk) {
      LOG_ERROR("Invalid shader type for Vulkan pipeline");
      return false;
    }

    VkPipelineShaderStageCreateInfo shaderStageInfo = {};
    shaderStageInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage                           = g_getShaderStageBitsVk(shaderVk->getStage());
    shaderStageInfo.module                          = shaderVk->getShaderModule();
    shaderStageInfo.pName                           = shaderVk->getEntryPoint().c_str();

    shaderStages.push_back(shaderStageInfo);
  }

  return !shaderStages.empty();
}

bool GraphicsPipelineVk::createVertexInputState_(VkPipelineVertexInputStateCreateInfo& vertexInputInfo) {
  // Static arrays to hold binding and attribute descriptions
  // Note: These must have a lifetime at least as long as the pipeline creation call
  static std::vector<VkVertexInputBindingDescription>   bindingDescriptions;
  static std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

  bindingDescriptions.clear();
  attributeDescriptions.clear();

  for (const auto& binding : m_desc_.vertexBindings) {
    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding                         = binding.binding;
    bindingDesc.stride                          = binding.stride;
    bindingDesc.inputRate                       = g_getVertexInputRateVk(binding.inputRate);

    bindingDescriptions.push_back(bindingDesc);
  }

  for (const auto& attribute : m_desc_.vertexAttributes) {
    VkVertexInputAttributeDescription attributeDesc = {};
    attributeDesc.location                          = attribute.location;
    attributeDesc.binding                           = attribute.binding;
    attributeDesc.format                            = g_getVertexFormatVk(attribute.format);
    attributeDesc.offset                            = attribute.offset;

    attributeDescriptions.push_back(attributeDesc);
  }

  vertexInputInfo                                 = {};
  vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindingDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions      = bindingDescriptions.data();
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

  return true;
}

bool GraphicsPipelineVk::createInputAssemblyState_(VkPipelineInputAssemblyStateCreateInfo& inputAssembly) {
  inputAssembly                        = {};
  inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology               = g_getPrimitiveTopologyVk(m_desc_.inputAssembly.topology);
  inputAssembly.primitiveRestartEnable = m_desc_.inputAssembly.primitiveRestartEnable ? VK_TRUE : VK_FALSE;

  return true;
}

bool GraphicsPipelineVk::createRasterizationState_(VkPipelineRasterizationStateCreateInfo& rasterizer) {
  rasterizer                         = {};
  rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable        = m_desc_.rasterization.depthClampEnable ? VK_TRUE : VK_FALSE;
  rasterizer.rasterizerDiscardEnable = m_desc_.rasterization.rasterizerDiscardEnable ? VK_TRUE : VK_FALSE;
  rasterizer.polygonMode             = g_getPolygonModeVk(m_desc_.rasterization.polygonMode);
  rasterizer.cullMode                = g_getCullModeVk(m_desc_.rasterization.cullMode);
  rasterizer.frontFace               = g_getFrontFaceVk(m_desc_.rasterization.frontFace);
  rasterizer.depthBiasEnable         = m_desc_.rasterization.depthBiasEnable ? VK_TRUE : VK_FALSE;
  rasterizer.depthBiasConstantFactor = m_desc_.rasterization.depthBiasConstantFactor;
  rasterizer.depthBiasClamp          = m_desc_.rasterization.depthBiasClamp;
  rasterizer.depthBiasSlopeFactor    = m_desc_.rasterization.depthBiasSlopeFactor;
  rasterizer.lineWidth               = m_desc_.rasterization.lineWidth;

  return true;
}

bool GraphicsPipelineVk::createMultisampleState_(VkPipelineMultisampleStateCreateInfo& multisampling) {
  multisampling       = {};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

  multisampling.rasterizationSamples
      = static_cast<VkSampleCountFlagBits>(g_getMSAASampleCount(m_desc_.multisample.rasterizationSamples));

  multisampling.sampleShadingEnable = m_desc_.multisample.sampleShadingEnable ? VK_TRUE : VK_FALSE;
  multisampling.minSampleShading    = m_desc_.multisample.minSampleShading;

  static uint32_t sampleMask = m_desc_.multisample.sampleMask;
  multisampling.pSampleMask  = &sampleMask;

  multisampling.alphaToCoverageEnable = m_desc_.multisample.alphaToCoverageEnable ? VK_TRUE : VK_FALSE;
  multisampling.alphaToOneEnable      = m_desc_.multisample.alphaToOneEnable ? VK_TRUE : VK_FALSE;

  return true;
}

bool GraphicsPipelineVk::createDepthStencilState_(VkPipelineDepthStencilStateCreateInfo& depthStencil) {
  depthStencil                       = {};
  depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable       = m_desc_.depthStencil.depthTestEnable ? VK_TRUE : VK_FALSE;
  depthStencil.depthWriteEnable      = m_desc_.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
  depthStencil.depthCompareOp        = g_getCompareOpVk(m_desc_.depthStencil.depthCompareOp);
  depthStencil.depthBoundsTestEnable = m_desc_.depthStencil.depthBoundsTestEnable ? VK_TRUE : VK_FALSE;
  depthStencil.stencilTestEnable     = m_desc_.depthStencil.stencilTestEnable ? VK_TRUE : VK_FALSE;

  depthStencil.front.failOp      = g_getStencilOpVk(m_desc_.depthStencil.front.failOp);
  depthStencil.front.passOp      = g_getStencilOpVk(m_desc_.depthStencil.front.passOp);
  depthStencil.front.depthFailOp = g_getStencilOpVk(m_desc_.depthStencil.front.depthFailOp);
  depthStencil.front.compareOp   = g_getCompareOpVk(m_desc_.depthStencil.front.compareOp);
  depthStencil.front.compareMask = m_desc_.depthStencil.front.compareMask;
  depthStencil.front.writeMask   = m_desc_.depthStencil.front.writeMask;
  depthStencil.front.reference   = m_desc_.depthStencil.front.reference;

  depthStencil.back.failOp      = g_getStencilOpVk(m_desc_.depthStencil.back.failOp);
  depthStencil.back.passOp      = g_getStencilOpVk(m_desc_.depthStencil.back.passOp);
  depthStencil.back.depthFailOp = g_getStencilOpVk(m_desc_.depthStencil.back.depthFailOp);
  depthStencil.back.compareOp   = g_getCompareOpVk(m_desc_.depthStencil.back.compareOp);
  depthStencil.back.compareMask = m_desc_.depthStencil.back.compareMask;
  depthStencil.back.writeMask   = m_desc_.depthStencil.back.writeMask;
  depthStencil.back.reference   = m_desc_.depthStencil.back.reference;

  depthStencil.minDepthBounds = m_desc_.depthStencil.minDepthBounds;
  depthStencil.maxDepthBounds = m_desc_.depthStencil.maxDepthBounds;

  return true;
}

bool GraphicsPipelineVk::createColorBlendState_(VkPipelineColorBlendStateCreateInfo& colorBlending) {
  static std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
  colorBlendAttachments.clear();

  for (const auto& attachment : m_desc_.colorBlend.attachments) {
    VkPipelineColorBlendAttachmentState attachmentState = {};
    attachmentState.blendEnable                         = attachment.blendEnable ? VK_TRUE : VK_FALSE;
    attachmentState.srcColorBlendFactor                 = g_getBlendFactorVk(attachment.srcColorBlendFactor);
    attachmentState.dstColorBlendFactor                 = g_getBlendFactorVk(attachment.dstColorBlendFactor);
    attachmentState.colorBlendOp                        = g_getBlendOpVk(attachment.colorBlendOp);
    attachmentState.srcAlphaBlendFactor                 = g_getBlendFactorVk(attachment.srcAlphaBlendFactor);
    attachmentState.dstAlphaBlendFactor                 = g_getBlendFactorVk(attachment.dstAlphaBlendFactor);
    attachmentState.alphaBlendOp                        = g_getBlendOpVk(attachment.alphaBlendOp);
    attachmentState.colorWriteMask                      = g_getColorMaskVk(attachment.colorWriteMask);

    colorBlendAttachments.push_back(attachmentState);
  }

  if (colorBlendAttachments.empty()) {
    LOG_WARN("No color blend attachments provided, using default attachment");
    VkPipelineColorBlendAttachmentState defaultAttachment = {};
    defaultAttachment.blendEnable                         = VK_FALSE;
    defaultAttachment.colorWriteMask
        = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachments.push_back(defaultAttachment);
  }

  colorBlending                 = {};
  colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable   = m_desc_.colorBlend.logicOpEnable ? VK_TRUE : VK_FALSE;
  colorBlending.logicOp         = g_getLogicOpVk(m_desc_.colorBlend.logicOp);
  colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
  colorBlending.pAttachments    = colorBlendAttachments.data();

  for (int i = 0; i < 4; i++) {
    colorBlending.blendConstants[i] = m_desc_.colorBlend.blendConstants[i];
  }

  return true;
}

bool GraphicsPipelineVk::createPipelineLayout_() {
  std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;

  for (const auto& setLayout : m_desc_.setLayouts) {
    const DescriptorSetLayoutVk* setLayoutVk = dynamic_cast<const DescriptorSetLayoutVk*>(setLayout);
    if (!setLayoutVk) {
      LOG_ERROR("Invalid descriptor set layout type for Vulkan pipeline");
      return false;
    }

    vkDescriptorSetLayouts.push_back(setLayoutVk->getLayout());
  }

  // TODO: add push constant ranges

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount             = static_cast<uint32_t>(vkDescriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts                = vkDescriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount     = 0;
  pipelineLayoutInfo.pPushConstantRanges        = nullptr;

  if (vkCreatePipelineLayout(m_device_->getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout_) != VK_SUCCESS) {
    LOG_ERROR("Failed to create pipeline layout");
    return false;
  }

  return true;
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise