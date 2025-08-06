#include "gfx/rhi/backends/vulkan/command_buffer_vk.h"

#include "gfx/rhi/backends/vulkan/buffer_vk.h"
#include "gfx/rhi/backends/vulkan/command_buffer_vk.h"
#include "gfx/rhi/backends/vulkan/descriptor_vk.h"
#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "gfx/rhi/backends/vulkan/framebuffer_vk.h"
#include "gfx/rhi/backends/vulkan/pipeline_vk.h"
#include "gfx/rhi/backends/vulkan/render_pass_vk.h"
#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"
#include "gfx/rhi/backends/vulkan/texture_vk.h"
#include "profiler/gpu.h"
#include "utils/color/color.h"
#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

//-------------------------------------------------------------------------
// CommandBufferVk implementation
//-------------------------------------------------------------------------

CommandBufferVk::CommandBufferVk(DeviceVk* device, VkCommandBuffer commandBuffer, VkCommandPool commandPool)
    : m_device_(device)
    , m_commandBuffer_(commandBuffer)
    , m_commandPool_(commandPool) {
}

void CommandBufferVk::begin() {
  if (m_isRecording_) {
    LOG_WARN("Command buffer is already recording");
    return;
  }

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags                    = 0;
  beginInfo.pInheritanceInfo         = nullptr;

  VkResult result = vkBeginCommandBuffer(m_commandBuffer_, &beginInfo);
  if (result != VK_SUCCESS) {
    LOG_ERROR("Failed to begin command buffer");
    return;
  }

  m_isRecording_ = true;
}

void CommandBufferVk::end() {
  if (!m_isRecording_) {
    LOG_WARN("Command buffer is not recording");
    return;
  }

  if (m_isRenderPassActive_) {
    endRenderPass();
  }

  VkResult result = vkEndCommandBuffer(m_commandBuffer_);
  if (result != VK_SUCCESS) {
    LOG_ERROR("Failed to end command buffer");
    return;
  }

  m_isRecording_ = false;
}

void CommandBufferVk::reset() {
  if (m_isRecording_) {
    LOG_WARN("Cannot reset while recording");
    return;
  }

  VkResult result = vkResetCommandBuffer(m_commandBuffer_, 0);
  if (result != VK_SUCCESS) {
    LOG_ERROR("Failed to reset command buffer");
    return;
  }

  m_currentPipeline_    = nullptr;
  m_currentRenderPass_  = nullptr;
  m_currentFramebuffer_ = nullptr;
  m_isRenderPassActive_ = false;
}

void CommandBufferVk::setPipeline(Pipeline* pipeline) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  auto pipelineVk = dynamic_cast<GraphicsPipelineVk*>(pipeline);
  if (!pipelineVk) {
    LOG_ERROR("Invalid pipeline type");
    return;
  }

  VkPipelineBindPoint bindPoint;
  switch (pipelineVk->getType()) {
    case PipelineType::Graphics:
      bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      break;
    case PipelineType::Compute:
      bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
      break;
    default:
      LOG_ERROR("Invalid pipeline type");
      return;
  }

  vkCmdBindPipeline(m_commandBuffer_, bindPoint, pipelineVk->getPipeline());

  m_currentPipeline_  = pipelineVk;
  m_currentBindPoint_ = bindPoint;
}

void CommandBufferVk::setViewport(const Viewport& viewport) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  VkViewport viewportVk = {};
  viewportVk.x          = viewport.x;
  viewportVk.y          = viewport.y + viewport.height;  // make Dx12 like
  viewportVk.width      = viewport.width;
  viewportVk.height     = -viewport.height;              // make Dx12 like
  viewportVk.minDepth   = viewport.minDepth;
  viewportVk.maxDepth   = viewport.maxDepth;

  vkCmdSetViewport(m_commandBuffer_, 0, 1, &viewportVk);
}

void CommandBufferVk::setScissor(const ScissorRect& scissor) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  VkRect2D scissorVk      = {};
  scissorVk.offset.x      = scissor.x;
  scissorVk.offset.y      = scissor.y;
  scissorVk.extent.width  = scissor.width;
  scissorVk.extent.height = scissor.height;

  vkCmdSetScissor(m_commandBuffer_, 0, 1, &scissorVk);
}

void CommandBufferVk::bindVertexBuffer(uint32_t binding, Buffer* buffer, uint64_t offset) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  BufferVk* bufferVk = dynamic_cast<BufferVk*>(buffer);
  if (!bufferVk) {
    LOG_ERROR("Invalid buffer type");
    return;
  }

  VkBuffer bufferHandle = bufferVk->getBuffer();
  vkCmdBindVertexBuffers(m_commandBuffer_, binding, 1, &bufferHandle, &offset);
}

void CommandBufferVk::bindIndexBuffer(Buffer* buffer, uint64_t offset, bool use32BitIndices) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  BufferVk* bufferVk = dynamic_cast<BufferVk*>(buffer);
  if (!bufferVk) {
    LOG_ERROR("Invalid buffer type");
    return;
  }

  VkIndexType indexType = use32BitIndices ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
  vkCmdBindIndexBuffer(m_commandBuffer_, bufferVk->getBuffer(), offset, indexType);
}

void CommandBufferVk::bindDescriptorSet(uint32_t setIndex, DescriptorSet* set) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  if (!m_currentPipeline_) {
    LOG_ERROR("No active pipeline");
    return;
  }

  // The code below demonstrates the naming convention used in this project:
  // - If the object is from one of our own classes, append the rendering API name as a postfix (e.g., descriptorSetVk).
  // - If the object comes from a third-party library, prepend the rendering API name (e.g., vkDescriptorSet).

  DescriptorSetVk* descriptorSetVk = dynamic_cast<DescriptorSetVk*>(set);
  if (!descriptorSetVk) {
    LOG_ERROR("Invalid descriptor set type");
    return;
  }

  VkDescriptorSet vkDescriptorSet = descriptorSetVk->getDescriptorSet();
  vkCmdBindDescriptorSets(m_commandBuffer_,
                          m_currentBindPoint_,
                          m_currentPipeline_->getPipelineLayout(),
                          setIndex,
                          1,
                          &vkDescriptorSet,
                          0,
                          nullptr);
}

void CommandBufferVk::draw(uint32_t vertexCount, uint32_t firstVertex) {
  if (!m_isRecording_ || !m_isRenderPassActive_) {
    LOG_ERROR("Command buffer is not recording or render pass is not active");
    return;
  }

  vkCmdDraw(m_commandBuffer_, vertexCount, 1, firstVertex, 0);
}

void CommandBufferVk::drawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
  if (!m_isRecording_ || !m_isRenderPassActive_) {
    LOG_ERROR("Command buffer is not recording or render pass is not active");
    return;
  }

  vkCmdDrawIndexed(m_commandBuffer_, indexCount, 1, firstIndex, vertexOffset, 0);
}

void CommandBufferVk::drawInstanced(uint32_t vertexCount,
                                    uint32_t instanceCount,
                                    uint32_t firstVertex,
                                    uint32_t firstInstance) {
  if (!m_isRecording_ || !m_isRenderPassActive_) {
    LOG_ERROR("Command buffer is not recording or render pass is not active");
    return;
  }

  vkCmdDraw(m_commandBuffer_, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBufferVk::drawIndexedInstanced(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
  if (!m_isRecording_ || !m_isRenderPassActive_) {
    LOG_ERROR("Command buffer is not recording or render pass is not active");
    return;
  }

  vkCmdDrawIndexed(m_commandBuffer_, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBufferVk::resourceBarrier(const ResourceBarrierDesc& barrier) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  if (!barrier.texture) {
    LOG_ERROR("Null texture");
    return;
  }

  TextureVk* textureVk = dynamic_cast<TextureVk*>(barrier.texture);
  if (!textureVk) {
    LOG_ERROR("Invalid texture type");
    return;
  }

  VkImageLayout oldLayout = g_getImageLayoutVk(barrier.oldLayout);
  VkImageLayout newLayout = g_getImageLayoutVk(barrier.newLayout);

  if (oldLayout == newLayout) {
    // TODO: uncomment this if needed
    // LOG_WARN("Skipping redundant barrier, states are identical");
    return;
  }

  VkImageMemoryBarrier imageBarrier = {};
  imageBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageBarrier.oldLayout            = oldLayout;
  imageBarrier.newLayout            = newLayout;
  imageBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
  imageBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
  imageBarrier.image                = textureVk->getImage();

  if (g_isDepthFormat(textureVk->getFormat())) {
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (!g_isDepthOnlyFormat(textureVk->getFormat())) {
      imageBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  imageBarrier.subresourceRange.baseMipLevel   = 0;
  imageBarrier.subresourceRange.levelCount     = textureVk->getMipLevels();
  imageBarrier.subresourceRange.baseArrayLayer = 0;
  imageBarrier.subresourceRange.layerCount     = textureVk->getArraySize();

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
    imageBarrier.srcAccessMask = 0;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    imageBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    imageBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  }

  if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  } else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  } else if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  } else if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    imageBarrier.dstAccessMask
        = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  } else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  } else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    imageBarrier.dstAccessMask = 0;
  }

  VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

  if (imageBarrier.srcAccessMask & VK_ACCESS_TRANSFER_WRITE_BIT) {
    srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (imageBarrier.srcAccessMask & VK_ACCESS_TRANSFER_READ_BIT) {
    srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (imageBarrier.srcAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) {
    srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else if (imageBarrier.srcAccessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) {
    srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  } else if (imageBarrier.srcAccessMask & VK_ACCESS_SHADER_READ_BIT) {
    srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }

  if (imageBarrier.dstAccessMask & VK_ACCESS_TRANSFER_WRITE_BIT) {
    dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (imageBarrier.dstAccessMask & VK_ACCESS_TRANSFER_READ_BIT) {
    dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (imageBarrier.dstAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) {
    dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else if (imageBarrier.dstAccessMask
             & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) {
    dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else if (imageBarrier.dstAccessMask & VK_ACCESS_SHADER_READ_BIT) {
    dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  }

  vkCmdPipelineBarrier(m_commandBuffer_, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

  textureVk->updateCurrentLayout_(barrier.newLayout);
}

void CommandBufferVk::beginRenderPass(RenderPass*                    renderPass,
                                      Framebuffer*                   framebuffer,
                                      const std::vector<ClearValue>& clearValues) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  if (m_isRenderPassActive_) {
    LOG_ERROR("Render pass is already active");
    return;
  }

  RenderPassVk*  renderPassVk  = dynamic_cast<RenderPassVk*>(renderPass);
  FramebufferVk* framebufferVk = dynamic_cast<FramebufferVk*>(framebuffer);

  if (!renderPassVk || !framebufferVk) {
    LOG_ERROR("Invalid render pass or framebuffer type");
    return;
  }

  framebufferVk->transitionToInitialLayouts(this, renderPassVk);

  std::vector<VkClearValue> clearValuesVk(clearValues.size());
  for (size_t i = 0; i < clearValues.size(); i++) {
    if (i < framebufferVk->getColorAttachmentCount()) {
      // color attachment
      memcpy(clearValuesVk[i].color.float32, clearValues[i].color, sizeof(float) * 4);
    } else {
      // depth/stencil attachment
      clearValuesVk[i].depthStencil.depth   = clearValues[i].depthStencil.depth;
      clearValuesVk[i].depthStencil.stencil = clearValues[i].depthStencil.stencil;
    }
  }

  VkRenderPassBeginInfo renderPassInfo = {};
  renderPassInfo.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass            = renderPassVk->getRenderPass();
  renderPassInfo.framebuffer           = framebufferVk->getFramebuffer();
  renderPassInfo.renderArea.offset     = {0, 0};
  renderPassInfo.renderArea.extent     = {framebufferVk->getWidth(), framebufferVk->getHeight()};
  renderPassInfo.clearValueCount       = static_cast<uint32_t>(clearValuesVk.size());
  renderPassInfo.pClearValues          = clearValuesVk.data();

  vkCmdBeginRenderPass(m_commandBuffer_, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  m_currentRenderPass_  = renderPassVk;
  m_currentFramebuffer_ = framebufferVk;
  m_isRenderPassActive_ = true;
}

void CommandBufferVk::endRenderPass() {
  if (!m_isRecording_ || !m_isRenderPassActive_) {
    LOG_ERROR("Command buffer is not recording or render pass is not active");
    return;
  }

  vkCmdEndRenderPass(m_commandBuffer_);

  if (m_currentFramebuffer_ && m_currentRenderPass_) {
    auto* framebufferVk = static_cast<FramebufferVk*>(m_currentFramebuffer_);
    framebufferVk->updateAttachmentLayouts(m_currentRenderPass_);
  }

  m_isRenderPassActive_ = false;
  m_currentRenderPass_  = nullptr;
  m_currentFramebuffer_ = nullptr;
}

void CommandBufferVk::copyBuffer(
    Buffer* srcBuffer, Buffer* dstBuffer, uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  BufferVk* srcBufferVk = dynamic_cast<BufferVk*>(srcBuffer);
  BufferVk* dstBufferVk = dynamic_cast<BufferVk*>(dstBuffer);

  if (!srcBufferVk || !dstBufferVk) {
    LOG_ERROR("Invalid buffer type");
    return;
  }

  // Determine copy size (if not specified, copy from offset to end of source buffer)
  VkDeviceSize copySize = static_cast<VkDeviceSize>(size);
  if (copySize == 0) {
    copySize = srcBufferVk->getSize() - srcOffset;
  }

  // Verify copy size doesn't exceed destination buffer
  if (dstOffset + copySize > dstBufferVk->getSize()) {
    LOG_ERROR("Copy operation exceeds destination buffer size");
    return;
  }

  VkBufferCopy copyRegion = {};
  copyRegion.srcOffset    = srcOffset;
  copyRegion.dstOffset    = dstOffset;
  copyRegion.size         = copySize;

  vkCmdCopyBuffer(m_commandBuffer_, srcBufferVk->getBuffer(), dstBufferVk->getBuffer(), 1, &copyRegion);
}

void CommandBufferVk::copyBufferToTexture(Buffer*  srcBuffer,
                                          Texture* dstTexture,
                                          uint32_t mipLevel,
                                          uint32_t arrayLayer) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  BufferVk*  srcBufferVk  = dynamic_cast<BufferVk*>(srcBuffer);
  TextureVk* dstTextureVk = dynamic_cast<TextureVk*>(dstTexture);

  if (!srcBufferVk || !dstTextureVk) {
    LOG_ERROR("Invalid buffer or texture type");
    return;
  }

  ResourceBarrierDesc barrier = {};
  barrier.texture             = dstTexture;
  barrier.oldLayout           = dstTextureVk->getCurrentLayoutType();
  barrier.newLayout           = ResourceLayout::TransferDst;
  resourceBarrier(barrier);

  uint32_t width = dstTextureVk->getWidth() >> mipLevel;
  width          = (width == 0) ? 1 : width;

  uint32_t height = dstTextureVk->getHeight() >> mipLevel;
  height          = (height == 0) ? 1 : height;

  uint32_t depth = dstTextureVk->getDepth() >> mipLevel;
  depth          = (depth == 0) ? 1 : depth;

  VkBufferImageCopy region = {};
  region.bufferOffset      = 0;
  region.bufferRowLength   = 0;  // Tightly packed
  region.bufferImageHeight = 0;  // Tightly packed

  region.imageSubresource.aspectMask
      = g_isDepthFormat(dstTextureVk->getFormat()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel       = mipLevel;
  region.imageSubresource.baseArrayLayer = arrayLayer;
  region.imageSubresource.layerCount     = 1;

  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, depth};

  vkCmdCopyBufferToImage(m_commandBuffer_,
                         srcBufferVk->getBuffer(),
                         dstTextureVk->getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &region);

  barrier.oldLayout = ResourceLayout::TransferDst;
  barrier.newLayout = dstTextureVk->getCurrentLayoutType();
  resourceBarrier(barrier);
}

void CommandBufferVk::copyTextureToBuffer(Texture* srcTexture,
                                          Buffer*  dstBuffer,
                                          uint32_t mipLevel,
                                          uint32_t arrayLayer) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  TextureVk* srcTextureVk = dynamic_cast<TextureVk*>(srcTexture);
  BufferVk*  dstBufferVk  = dynamic_cast<BufferVk*>(dstBuffer);

  if (!srcTextureVk || !dstBufferVk) {
    LOG_ERROR("Invalid texture or buffer type");
    return;
  }

  ResourceBarrierDesc barrier = {};
  barrier.texture             = srcTexture;
  barrier.oldLayout           = srcTextureVk->getCurrentLayoutType();
  barrier.newLayout           = ResourceLayout::TransferSrc;
  resourceBarrier(barrier);

  // Calculate subresource dimensions
  uint32_t width = srcTextureVk->getWidth() >> mipLevel;
  width          = (width == 0) ? 1 : width;

  uint32_t height = srcTextureVk->getHeight() >> mipLevel;
  height          = (height == 0) ? 1 : height;

  uint32_t depth = srcTextureVk->getDepth() >> mipLevel;
  depth          = (depth == 0) ? 1 : depth;

  // Setup image to buffer copy
  VkBufferImageCopy region = {};
  region.bufferOffset      = 0;
  region.bufferRowLength   = 0;  // Tightly packed
  region.bufferImageHeight = 0;  // Tightly packed

  region.imageSubresource.aspectMask
      = g_isDepthFormat(srcTextureVk->getFormat()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel       = mipLevel;
  region.imageSubresource.baseArrayLayer = arrayLayer;
  region.imageSubresource.layerCount     = 1;

  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, depth};

  vkCmdCopyImageToBuffer(m_commandBuffer_,
                         srcTextureVk->getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         dstBufferVk->getBuffer(),
                         1,
                         &region);

  barrier.oldLayout = ResourceLayout::TransferSrc;
  barrier.newLayout = srcTextureVk->getCurrentLayoutType();
  resourceBarrier(barrier);
}

void CommandBufferVk::copyTexture(Texture* srcTexture,
                                  Texture* dstTexture,
                                  uint32_t srcMipLevel,
                                  uint32_t srcArrayLayer,
                                  uint32_t dstMipLevel,
                                  uint32_t dstArrayLayer) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  auto* srcTexVk = dynamic_cast<TextureVk*>(srcTexture);
  auto* dstTexVk = dynamic_cast<TextureVk*>(dstTexture);
  if (!srcTexVk || !dstTexVk) {
    LOG_ERROR("Invalid texture type");
    return;
  }

  ResourceLayout srcInitialLayout = srcTexVk->getCurrentLayoutType();
  ResourceLayout dstInitialLayout = dstTexVk->getCurrentLayoutType();

  // barrier: SRC -> TRANSFER_SRC, DST -> TRANSFER_DST
  ResourceBarrierDesc barrier{};
  barrier.texture   = srcTexture;
  barrier.oldLayout = srcTexVk->getCurrentLayoutType();
  barrier.newLayout = ResourceLayout::TransferSrc;
  resourceBarrier(barrier);

  barrier.texture   = dstTexture;
  barrier.oldLayout = dstTexVk->getCurrentLayoutType();
  barrier.newLayout = ResourceLayout::TransferDst;
  resourceBarrier(barrier);

  VkImageCopy region{};
  region.srcSubresource.aspectMask
      = g_isDepthFormat(srcTexVk->getFormat()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  region.srcSubresource.mipLevel       = srcMipLevel;
  region.srcSubresource.baseArrayLayer = srcArrayLayer;
  region.srcSubresource.layerCount     = 1;

  region.dstSubresource.aspectMask
      = g_isDepthFormat(dstTexVk->getFormat()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  region.dstSubresource.mipLevel       = dstMipLevel;
  region.dstSubresource.baseArrayLayer = dstArrayLayer;
  region.dstSubresource.layerCount     = 1;

  // full sub‑resource
  region.extent.width  = std::max<uint32_t>(1, dstTexVk->getWidth() >> dstMipLevel);
  region.extent.height = std::max<uint32_t>(1, dstTexVk->getHeight() >> dstMipLevel);
  region.extent.depth  = std::max<uint32_t>(1, dstTexVk->getDepth() >> dstMipLevel);

  vkCmdCopyImage(m_commandBuffer_,
                 srcTexVk->getImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dstTexVk->getImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 1,
                 &region);

  // restore original layouts
  barrier.texture   = srcTexture;
  barrier.oldLayout = ResourceLayout::TransferSrc;
  barrier.newLayout = srcInitialLayout;
  resourceBarrier(barrier);

  barrier.texture   = dstTexture;
  barrier.oldLayout = ResourceLayout::TransferDst;
  barrier.newLayout = dstInitialLayout;
  resourceBarrier(barrier);
}

void CommandBufferVk::clearColor(Texture* texture, const float color[4], uint32_t mipLevel, uint32_t arrayLayer) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  TextureVk* textureVk = dynamic_cast<TextureVk*>(texture);
  if (!textureVk) {
    LOG_ERROR("Invalid texture type");
    return;
  }

  ResourceBarrierDesc barrier = {};
  barrier.texture             = texture;
  barrier.oldLayout           = textureVk->getCurrentLayoutType();
  barrier.newLayout           = ResourceLayout::TransferDst;
  resourceBarrier(barrier);

  VkClearColorValue clearColor = {};
  memcpy(clearColor.float32, color, sizeof(float) * 4);

  VkImageSubresourceRange range = {};
  range.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
  range.baseMipLevel            = mipLevel;
  range.levelCount              = 1;
  range.baseArrayLayer          = arrayLayer;
  range.layerCount              = 1;

  vkCmdClearColorImage(
      m_commandBuffer_, textureVk->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

  barrier.oldLayout = ResourceLayout::TransferDst;
  barrier.newLayout = textureVk->getCurrentLayoutType();
  resourceBarrier(barrier);
}

void CommandBufferVk::clearDepthStencil(
    Texture* texture, float depth, uint8_t stencil, uint32_t mipLevel, uint32_t arrayLayer) {
  if (!m_isRecording_) {
    LOG_ERROR("Command buffer is not recording");
    return;
  }

  TextureVk* textureVk = dynamic_cast<TextureVk*>(texture);
  if (!textureVk) {
    LOG_ERROR("Invalid texture type");
    return;
  }

  if (!g_isDepthFormat(textureVk->getFormat())) {
    LOG_ERROR("Texture is not a depth/stencil texture");
    return;
  }

  ResourceBarrierDesc barrier = {};
  barrier.texture             = texture;
  barrier.oldLayout           = textureVk->getCurrentLayoutType();
  barrier.newLayout           = ResourceLayout::TransferDst;
  resourceBarrier(barrier);

  VkClearDepthStencilValue clearValue = {};
  clearValue.depth                    = depth;
  clearValue.stencil                  = stencil;

  VkImageSubresourceRange range = {};
  range.aspectMask              = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (!g_isDepthOnlyFormat(textureVk->getFormat())) {
    range.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  range.baseMipLevel   = mipLevel;
  range.levelCount     = 1;
  range.baseArrayLayer = arrayLayer;
  range.layerCount     = 1;

  vkCmdClearDepthStencilImage(
      m_commandBuffer_, textureVk->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);

  barrier.oldLayout = ResourceLayout::TransferDst;
  barrier.newLayout = textureVk->getCurrentLayoutType();
  resourceBarrier(barrier);
}

void CommandBufferVk::beginDebugMarker(const std::string& name, const float color[4]) {
  // TODO: i guess that this function should be in device and we need to retrieve it from device
  auto func
      = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(m_device_->getDevice(), "vkCmdBeginDebugUtilsLabelEXT");

  if (!func) {
    LOG_ERROR("Debug marker function not supported");
    return;
  }

  VkDebugUtilsLabelEXT label{};
  label.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  label.pLabelName = name.c_str();
  if (color) {
    memcpy(label.color, color, sizeof(float) * 4);
  } else {
    auto defaultColor = color::g_toFloatArray(color::GREEN);
    label.color[0]    = defaultColor[0];
    label.color[1]    = defaultColor[1];
    label.color[2]    = defaultColor[2];
    label.color[3]    = defaultColor[3];
  }

  func(m_commandBuffer_, &label);
}

void CommandBufferVk::endDebugMarker() {
  auto func = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(m_device_->getDevice(), "vkCmdEndDebugUtilsLabelEXT");

  if (func) {
    func(m_commandBuffer_);
  }
}

void CommandBufferVk::insertDebugMarker(const std::string& name, const float color[4]) {
  auto func
      = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(m_device_->getDevice(), "vkCmdInsertDebugUtilsLabelEXT");

  if (!func) {
    LOG_ERROR("Debug marker function not supported");
    return;
  }

  VkDebugUtilsLabelEXT label{};
  label.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  label.pLabelName = name.c_str();

  if (color) {
    memcpy(label.color, color, sizeof(float) * 4);
  } else {
    auto defaultColor = color::g_toFloatArray(color::GREEN);
    label.color[0]    = defaultColor[0];
    label.color[1]    = defaultColor[1];
    label.color[2]    = defaultColor[2];
    label.color[3]    = defaultColor[3];
  }

  func(m_commandBuffer_, &label);
}

//-------------------------------------------------------------------------
// CommandPoolManager implementation
//-------------------------------------------------------------------------

CommandPoolManager::~CommandPoolManager() {
  release();
}

bool CommandPoolManager::initialize(VkDevice device, uint32_t queueFamilyIndex) {
  release();

  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex        = queueFamilyIndex;

  VkCommandPool newPool = VK_NULL_HANDLE;
  if (vkCreateCommandPool(device, &poolInfo, nullptr, &newPool) != VK_SUCCESS) {
    return false;
  }

  m_device_           = device;
  m_queueFamilyIndex_ = queueFamilyIndex;
  m_mainCommandPool   = newPool;
  m_mainThreadId      = std::this_thread::get_id();

  m_isInitialized = true;

  return true;
}

void CommandPoolManager::release() {
  cleanupThreadPools();

  if (m_device_ && m_mainCommandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_device_, m_mainCommandPool, nullptr);
    m_mainCommandPool = VK_NULL_HANDLE;
  }

  m_device_           = VK_NULL_HANDLE;
  m_queueFamilyIndex_ = 0;
  m_isInitialized     = false;
}

VkCommandPool CommandPoolManager::getPool() const {
  if (std::this_thread::get_id() == m_mainThreadId) {
    return m_mainCommandPool;
  }

  std::thread::id threadId = std::this_thread::get_id();

  {
    std::lock_guard<std::mutex> lock(m_threadPoolsMutex);
    auto                        it = m_threadPools.find(threadId);
    if (it != m_threadPools.end()) {
      return it->second;
    }
  }

  VkCommandPool pool = createThreadPool();

  {
    std::lock_guard<std::mutex> lock(m_threadPoolsMutex);
    m_threadPools[threadId] = pool;
  }

  return pool;
}

VkCommandPool CommandPoolManager::createThreadPool() const {
  if (!m_isInitialized || m_device_ == VK_NULL_HANDLE) {
    LOG_ERROR("Cannot create thread pool: CommandPoolManager not initialized");
    return VK_NULL_HANDLE;
  }

  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex        = m_queueFamilyIndex_;

  VkCommandPool pool = VK_NULL_HANDLE;
  if (vkCreateCommandPool(m_device_, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
    LOG_ERROR("Failed to create thread-specific command pool");
    return m_mainCommandPool;
  }

  registerThreadPool(pool);

  return pool;
}

void CommandPoolManager::registerThreadPool(VkCommandPool pool) const {
  std::thread::id threadId = std::this_thread::get_id();

  std::lock_guard<std::mutex> lock(m_threadPoolsMutex);
  m_threadPools[threadId] = pool;
}

void CommandPoolManager::cleanupThreadPools() {
  std::lock_guard<std::mutex> lock(m_threadPoolsMutex);

  for (auto& pair : m_threadPools) {
    if (m_device_ && pair.second != VK_NULL_HANDLE) {
      vkDestroyCommandPool(m_device_, pair.second, nullptr);
    }
  }

  m_threadPools.clear();
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise