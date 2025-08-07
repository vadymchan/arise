#include "gfx/rhi/backends/dx12/render_pass_dx12.h"

#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "gfx/rhi/backends/dx12/rhi_enums_dx12.h"
#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

RenderPassDx12::RenderPassDx12(const RenderPassDesc& desc, DeviceDx12* device)
    : RenderPass(desc)
    , m_device_(device)
    , m_hasDepthStencil_(desc.hasDepthStencil) {
  if (!initialize_(desc)) {
    LOG_ERROR("Failed to initialize render pass");
  }
}

bool RenderPassDx12::initialize_(const RenderPassDesc& desc) {
  m_colorFormats_.reserve(desc.colorAttachments.size());
  m_colorAttachmentOps_.resize(desc.colorAttachments.size());

  for (size_t i = 0; i < desc.colorAttachments.size(); i++) {
    m_colorFormats_.push_back(g_getTextureFormatDx12(desc.colorAttachments[i].format));
    m_colorAttachmentOps_[i] = desc.colorAttachments[i].loadStoreOp;
  }

  if (desc.hasDepthStencil) {
    m_depthStencilFormat_       = g_getTextureFormatDx12(desc.depthStencilAttachment.format);
    m_depthStencilAttachmentOp_ = desc.depthStencilAttachment.loadStoreOp;
  }

  m_colorFinalLayouts.resize(desc.colorAttachments.size());
  for (size_t i = 0; i < desc.colorAttachments.size(); i++) {
    m_colorFinalLayouts[i] = desc.colorAttachments[i].finalLayout;
  }

  if (desc.hasDepthStencil) {
    m_depthStencilFinalLayout = desc.depthStencilAttachment.finalLayout;
  }

  return true;
}

ResourceLayout RenderPassDx12::getFinalColorAttachmentLayout(uint32_t index) const {
  if (index < m_colorFinalLayouts.size()) {
    return m_colorFinalLayouts[index];
  }

  LOG_ERROR("Invalid color attachment index: {}", index);
  return ResourceLayout::ShaderReadOnly;
}

bool RenderPassDx12::shouldClearColor(uint32_t attachmentIndex) const {
  if (attachmentIndex >= m_colorAttachmentOps_.size()) {
    return false;
  }

  return m_colorAttachmentOps_[attachmentIndex] == AttachmentLoadStoreOp::ClearStore
      || m_colorAttachmentOps_[attachmentIndex] == AttachmentLoadStoreOp::ClearDontcare;
}

bool RenderPassDx12::shouldClearDepthStencil() const {
  if (!m_hasDepthStencil_) {
    return false;
  }

  return m_depthStencilAttachmentOp_ == AttachmentLoadStoreOp::ClearStore
      || m_depthStencilAttachmentOp_ == AttachmentLoadStoreOp::ClearDontcare;
}

bool RenderPassDx12::shouldClearStencil() const {
  // For simplicity, we're using the same load operation for both depth and stencil
  // TODO: more sophisticated implementation that might track them separately
  return shouldClearDepthStencil() && !g_isDepthOnlyFormat(g_getTextureFormatDx12(m_depthStencilFormat_));
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise