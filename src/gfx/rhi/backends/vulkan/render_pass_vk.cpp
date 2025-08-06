#include "gfx/rhi/backends/vulkan/render_pass_vk.h"

#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"
#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

RenderPassVk::RenderPassVk(const RenderPassDesc& desc, DeviceVk* device)
    : RenderPass(desc)
    , m_device_(device)
    , m_hasDepthStencil_(desc.hasDepthStencil) {
  if (!initialize_(desc)) {
    LOG_ERROR("Failed to initialize render pass");
  }

  m_colorAttachmentInitialLayouts.resize(desc.colorAttachments.size());
  for (size_t i = 0; i < desc.colorAttachments.size(); i++) {
    m_colorAttachmentInitialLayouts[i] = desc.colorAttachments[i].initialLayout;
  }

  if (desc.hasDepthStencil) {
    m_depthAttachmentStencilInitialLayout = desc.depthStencilAttachment.initialLayout;
  }

  m_colorAttachmentFinalLayouts.resize(desc.colorAttachments.size());
  for (size_t i = 0; i < desc.colorAttachments.size(); i++) {
    m_colorAttachmentFinalLayouts[i] = desc.colorAttachments[i].finalLayout;
  }

  if (desc.hasDepthStencil) {
    m_depthStencilFinalLayout = desc.depthStencilAttachment.finalLayout;
  }
}

RenderPassVk::~RenderPassVk() {
  if (m_renderPass_ != VK_NULL_HANDLE && m_device_) {
    vkDestroyRenderPass(m_device_->getDevice(), m_renderPass_, nullptr);
    m_renderPass_ = VK_NULL_HANDLE;
  }
}

bool RenderPassVk::initialize_(const RenderPassDesc& desc) {
  // Store load/store operations for shouldClear* methods
  m_colorAttachmentOps_.resize(desc.colorAttachments.size());
  for (size_t i = 0; i < desc.colorAttachments.size(); i++) {
    m_colorAttachmentOps_[i] = desc.colorAttachments[i].loadStoreOp;
  }

  if (desc.hasDepthStencil) {
    m_depthStencilAttachmentOp_ = desc.depthStencilAttachment.loadStoreOp;
  }

  std::vector<VkAttachmentDescription> attachmentDescs;
  attachmentDescs.reserve(desc.colorAttachments.size() + (desc.hasDepthStencil ? 1 : 0));

  for (const auto& colorAttachment : desc.colorAttachments) {
    VkAttachmentDescription attachmentDesc = {};
    attachmentDesc.format                  = g_getTextureFormatVk(colorAttachment.format);
    attachmentDesc.samples                 = static_cast<VkSampleCountFlagBits>(colorAttachment.samples);

    VkAttachmentLoadOp  loadOp;
    VkAttachmentStoreOp storeOp;
    g_getAttachmentLoadStoreOpVk(loadOp, storeOp, colorAttachment.loadStoreOp);

    attachmentDesc.loadOp         = loadOp;
    attachmentDesc.storeOp        = storeOp;
    attachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDesc.initialLayout  = g_getImageLayoutVk(colorAttachment.initialLayout);
    attachmentDesc.finalLayout    = g_getImageLayoutVk(colorAttachment.finalLayout);

    attachmentDescs.push_back(attachmentDesc);
  }

  VkAttachmentDescription depthAttachmentDesc = {};
  if (desc.hasDepthStencil) {
    depthAttachmentDesc.format  = g_getTextureFormatVk(desc.depthStencilAttachment.format);
    depthAttachmentDesc.samples = static_cast<VkSampleCountFlagBits>(desc.depthStencilAttachment.samples);

    VkAttachmentLoadOp  loadOp, stencilLoadOp;
    VkAttachmentStoreOp storeOp, stencilStoreOp;
    g_getAttachmentLoadStoreOpVk(loadOp, storeOp, desc.depthStencilAttachment.loadStoreOp);
    g_getAttachmentLoadStoreOpVk(stencilLoadOp, stencilStoreOp, desc.depthStencilAttachment.stencilLoadStoreOp);

    depthAttachmentDesc.loadOp         = loadOp;
    depthAttachmentDesc.storeOp        = storeOp;
    depthAttachmentDesc.stencilLoadOp  = stencilLoadOp;
    depthAttachmentDesc.stencilStoreOp = stencilStoreOp;
    depthAttachmentDesc.initialLayout  = g_getImageLayoutVk(desc.depthStencilAttachment.initialLayout);
    depthAttachmentDesc.finalLayout    = g_getImageLayoutVk(desc.depthStencilAttachment.finalLayout);

    attachmentDescs.push_back(depthAttachmentDesc);
  }

  std::vector<VkAttachmentReference> colorAttachmentRefs;
  colorAttachmentRefs.reserve(desc.colorAttachments.size());

  for (size_t i = 0; i < desc.colorAttachments.size(); i++) {
    VkAttachmentReference attachmentRef = {};
    attachmentRef.attachment            = static_cast<uint32_t>(i);
    attachmentRef.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentRefs.push_back(attachmentRef);
  }

  VkAttachmentReference depthAttachmentRef = {};
  if (desc.hasDepthStencil) {
    depthAttachmentRef.attachment = static_cast<uint32_t>(desc.colorAttachments.size());
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }

  // just one subpass for now
  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
  subpass.pColorAttachments    = colorAttachmentRefs.data();

  if (desc.hasDepthStencil) {
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
  }

  VkSubpassDependency dependency = {};
  dependency.srcSubpass          = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass          = 0;
  dependency.srcStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                          | (desc.hasDepthStencil ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT : 0);
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                          | (desc.hasDepthStencil ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT : 0);
  dependency.srcAccessMask = 0;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                           | (desc.hasDepthStencil ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0);
  dependency.dependencyFlags = 0;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount        = static_cast<uint32_t>(attachmentDescs.size());
  renderPassInfo.pAttachments           = attachmentDescs.data();
  renderPassInfo.subpassCount           = 1;
  renderPassInfo.pSubpasses             = &subpass;
  renderPassInfo.dependencyCount        = 1;
  renderPassInfo.pDependencies          = &dependency;

  VkResult result = vkCreateRenderPass(m_device_->getDevice(), &renderPassInfo, nullptr, &m_renderPass_);
  if (result != VK_SUCCESS) {
    LOG_ERROR("Failed to create render pass");
    return false;
  }

  return true;
}

ResourceLayout RenderPassVk::getColorAttachmentInitialLayout(uint32_t index) const {
  if (index < m_colorAttachmentInitialLayouts.size()) {
    return m_colorAttachmentInitialLayouts[index];
  }
  return ResourceLayout::Undefined;
}

ResourceLayout RenderPassVk::getDepthStencilAttachmentInitialLayout() const {
  return m_depthAttachmentStencilInitialLayout;
}

ResourceLayout RenderPassVk::getColorAttachmentFinalLayout(uint32_t index) const {
  if (index < m_colorAttachmentFinalLayouts.size()) {
    return m_colorAttachmentFinalLayouts[index];
  }

  LOG_WARN("Invalid color attachment index for final layout");
  return ResourceLayout::ColorAttachment;
}

ResourceLayout RenderPassVk::getDepthStencilAttachmentFinalLayout() const {
  if (!m_hasDepthStencil_) {
    LOG_WARN("No depth/stencil attachment");
    return ResourceLayout::Undefined;
  }

  return m_depthStencilFinalLayout;
}

bool RenderPassVk::shouldClearColor(uint32_t attachmentIndex) const {
  if (attachmentIndex >= m_colorAttachmentOps_.size()) {
    return false;
  }

  return m_colorAttachmentOps_[attachmentIndex] == AttachmentLoadStoreOp::ClearStore
      || m_colorAttachmentOps_[attachmentIndex] == AttachmentLoadStoreOp::ClearDontcare;
}

bool RenderPassVk::shouldClearDepthStencil() const {
  if (!m_hasDepthStencil_) {
    return false;
  }

  return m_depthStencilAttachmentOp_ == AttachmentLoadStoreOp::ClearStore
      || m_depthStencilAttachmentOp_ == AttachmentLoadStoreOp::ClearDontcare;
}

bool RenderPassVk::shouldClearStencil() const {
  // using the same load operation for both depth and stencil (for simplicity)
  return shouldClearDepthStencil();
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise