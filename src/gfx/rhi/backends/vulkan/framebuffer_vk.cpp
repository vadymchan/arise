#include "gfx/rhi/backends/vulkan/framebuffer_vk.h"

#include "gfx/rhi/backends/vulkan/command_buffer_vk.h"
#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "gfx/rhi/backends/vulkan/render_pass_vk.h"
#include "gfx/rhi/backends/vulkan/texture_vk.h"
#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

FramebufferVk::FramebufferVk(const FramebufferDesc& desc, DeviceVk* device)
    : m_device(device)
    , m_width(desc.width)
    , m_height(desc.height)
    , m_hasDepthStencil(desc.hasDepthStencil) {
  if (!initialize(desc)) {
    LOG_ERROR("Failed to initialize framebuffer");
  }
}

FramebufferVk::~FramebufferVk() {
  if (m_framebuffer != VK_NULL_HANDLE && m_device) {
    vkDestroyFramebuffer(m_device->getDevice(), m_framebuffer, nullptr);
    m_framebuffer = VK_NULL_HANDLE;
  }
}

void FramebufferVk::transitionToInitialLayouts(CommandBufferVk* cmdBuffer, RenderPassVk* renderPass) {
  for (uint32_t i = 0; i < m_colorAttachments.size(); i++) {
    if (m_colorAttachments[i]) {
      TextureVk*     textureVk     = static_cast<TextureVk*>(m_colorAttachments[i]);
      ResourceLayout currentLayout = textureVk->getCurrentLayoutType();
      ResourceLayout initialLayout = renderPass->getColorAttachmentInitialLayout(i);

      if (currentLayout != initialLayout) {
        ResourceBarrierDesc barrier;
        barrier.texture   = m_colorAttachments[i];
        barrier.oldLayout = currentLayout;
        barrier.newLayout = initialLayout;
        cmdBuffer->resourceBarrier(barrier);
      }
    }
  }

  if (m_depthStencilAttachment) {
    TextureVk*     textureVk     = static_cast<TextureVk*>(m_depthStencilAttachment);
    ResourceLayout currentLayout = textureVk->getCurrentLayoutType();
    ResourceLayout initialLayout = renderPass->getDepthStencilAttachmentInitialLayout();

    if (currentLayout != initialLayout) {
      ResourceBarrierDesc barrier;
      barrier.texture   = m_depthStencilAttachment;
      barrier.oldLayout = currentLayout;
      barrier.newLayout = initialLayout;
      cmdBuffer->resourceBarrier(barrier);
    }
  }
}

void FramebufferVk::updateAttachmentLayouts(RenderPass* renderPass) {
  RenderPassVk* renderPassVk = static_cast<RenderPassVk*>(renderPass);

  for (uint32_t i = 0; i < m_colorAttachments.size(); i++) {
    if (m_colorAttachments[i]) {
      TextureVk*     textureVk   = static_cast<TextureVk*>(m_colorAttachments[i]);
      ResourceLayout finalLayout = renderPassVk->getColorAttachmentFinalLayout(i);
      textureVk->updateCurrentLayout_(finalLayout);
    }
  }

  if (m_depthStencilAttachment) {
    TextureVk*     textureVk   = static_cast<TextureVk*>(m_depthStencilAttachment);
    ResourceLayout finalLayout = renderPassVk->getDepthStencilAttachmentFinalLayout();
    textureVk->updateCurrentLayout_(finalLayout);
  }
}

bool FramebufferVk::initialize(const FramebufferDesc& desc) {
  m_colorAttachments.clear();
  for (auto* texture : desc.colorAttachments) {
    TextureVk* textureVk = dynamic_cast<TextureVk*>(texture);
    if (!textureVk) {
      LOG_ERROR("Invalid texture type for color attachment");
      return false;
    }
    m_colorAttachments.push_back(textureVk);
  }

  if (desc.hasDepthStencil && desc.depthStencilAttachment) {
    m_depthStencilAttachment = dynamic_cast<TextureVk*>(desc.depthStencilAttachment);
    if (!m_depthStencilAttachment) {
      LOG_ERROR("Invalid texture type for depth/stencil attachment");
      return false;
    }
  }

  RenderPassVk* renderPassVk = dynamic_cast<RenderPassVk*>(desc.renderPass);
  if (!renderPassVk) {
    LOG_ERROR("Invalid render pass type");
    return false;
  }

  std::vector<VkImageView> attachmentViews;
  attachmentViews.reserve(m_colorAttachments.size() + (m_hasDepthStencil ? 1 : 0));

  for (auto* texture : m_colorAttachments) {
    attachmentViews.push_back(texture->getImageView());
  }

  if (m_hasDepthStencil && m_depthStencilAttachment) {
    attachmentViews.push_back(m_depthStencilAttachment->getImageView());
  }

  VkFramebufferCreateInfo framebufferInfo = {};
  framebufferInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass              = renderPassVk->getRenderPass();
  framebufferInfo.attachmentCount         = static_cast<uint32_t>(attachmentViews.size());
  framebufferInfo.pAttachments            = attachmentViews.data();
  framebufferInfo.width                   = m_width;
  framebufferInfo.height                  = m_height;
  framebufferInfo.layers                  = 1;  // Could be more for multi-layered rendering

  VkResult result = vkCreateFramebuffer(m_device->getDevice(), &framebufferInfo, nullptr, &m_framebuffer);
  if (result != VK_SUCCESS) {
    LOG_ERROR("Failed to create framebuffer");
    return false;
  }

  return true;
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise