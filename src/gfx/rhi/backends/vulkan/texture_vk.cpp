#include "gfx/rhi/backends/vulkan/texture_vk.h"

#include "gfx/rhi/backends/vulkan/command_buffer_vk.h"
#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"
#include "gfx/rhi/backends/vulkan/synchronization_vk.h"
#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

TextureVk::TextureVk(const TextureDesc& desc, DeviceVk* device)
    : Texture(desc)
    , m_device_(device) {
  m_vkFormat_ = g_getTextureFormatVk(desc.format);

  m_currentLayout_ = ResourceLayout::Undefined;  // initially vulkan texture is in undefined layout

  if (!createImage_() || !createImageView_()) {
    LOG_ERROR("Failed to create Vulkan texture");
  }

  if (!desc.debugName.empty() && m_image_ != VK_NULL_HANDLE) {
    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType   = VK_OBJECT_TYPE_IMAGE;
    nameInfo.objectHandle = (uint64_t)m_image_;
    nameInfo.pObjectName  = desc.debugName.c_str();

    auto fpSetDebugUtilsObjectName
        = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(m_device_->getDevice(), "vkSetDebugUtilsObjectNameEXT");

    if (fpSetDebugUtilsObjectName) {
      fpSetDebugUtilsObjectName(m_device_->getDevice(), &nameInfo);
    }
  }
}

TextureVk::TextureVk(DeviceVk* device, const TextureDesc& desc, VkImage existingImage, VkImageView existingImageView)
    : Texture(desc)
    , m_device_(device)
    , m_image_(existingImage)
    , m_imageView_(existingImageView)
    , m_ownsResources_(false) {
  m_vkFormat_ = g_getTextureFormatVk(desc.format);

  m_currentLayout_ = desc.initialLayout;
}

TextureVk::~TextureVk() {
  if (m_device_) {
    VkDevice device = m_device_->getDevice();

    if (m_ownsResources_) {
      if (m_imageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_imageView_, nullptr);
        m_imageView_ = VK_NULL_HANDLE;
      }

      if (m_image_ != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device_->getAllocator(), m_image_, m_allocation_);
        m_image_      = VK_NULL_HANDLE;
        m_allocation_ = VK_NULL_HANDLE;
      }
    }
  }
}

void TextureVk::transitionToInitialLayout(CommandBuffer* cmdBuffer) {
  if (m_desc_.initialLayout != ResourceLayout::Undefined) {
    ResourceBarrierDesc barrier;
    barrier.texture   = this;
    barrier.oldLayout = ResourceLayout::Undefined;
    barrier.newLayout = m_desc_.initialLayout;
    cmdBuffer->resourceBarrier(barrier);

    m_currentLayout_ = m_desc_.initialLayout;
  }
}

bool TextureVk::createImage_() {
  VkImageCreateInfo imageInfo = {};
  imageInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

  switch (m_desc_.type) {
    case TextureType::Texture1D:
    case TextureType::Texture1DArray:
      imageInfo.imageType = VK_IMAGE_TYPE_1D;
      break;
    case TextureType::Texture2D:
    case TextureType::Texture2DArray:
    case TextureType::TextureCube:
      imageInfo.imageType = VK_IMAGE_TYPE_2D;
      break;
    case TextureType::Texture3D:
    case TextureType::Texture3DArray:
      imageInfo.imageType = VK_IMAGE_TYPE_3D;
      break;
    default:
      LOG_ERROR("Unsupported texture type");
      return false;
  }

  imageInfo.format        = m_vkFormat_;
  imageInfo.extent.width  = m_desc_.width;
  imageInfo.extent.height = m_desc_.height;
  imageInfo.extent.depth  = m_desc_.depth;
  imageInfo.mipLevels     = m_desc_.mipLevels;
  imageInfo.arrayLayers   = m_desc_.arraySize;

  if (m_desc_.type == TextureType::TextureCube) {
    imageInfo.arrayLayers  = 6;
    imageInfo.flags       |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  }

  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  if (m_desc_.sampleCount == MSAASamples::Count2) {
    imageInfo.samples = VK_SAMPLE_COUNT_2_BIT;
  } else if (m_desc_.sampleCount == MSAASamples::Count4) {
    imageInfo.samples = VK_SAMPLE_COUNT_4_BIT;
  } else if (m_desc_.sampleCount == MSAASamples::Count8) {
    imageInfo.samples = VK_SAMPLE_COUNT_8_BIT;
  } else if (m_desc_.sampleCount == MSAASamples::Count16) {
    imageInfo.samples = VK_SAMPLE_COUNT_16_BIT;
  }

  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

  imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

  if (hasRtvUsage()) {
    imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  if (hasDsvUsage()) {
    imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }

  if (hasUavUsage()) {
    imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  }

  imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  if (m_desc_.mipLevels > 1 || ((m_desc_.createFlags & TextureCreateFlag::TransferSrc) != TextureCreateFlag::None)) {
    imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;  // Textures typically use GPU-only memory

  VkResult result = vmaCreateImage(
      m_device_->getAllocator(), &imageInfo, &allocInfo, &m_image_, &m_allocation_, &m_allocationInfo_);

  if (result != VK_SUCCESS) {
    LOG_ERROR("Failed to create image with VMA");
    return false;
  }

  return true;
}

bool TextureVk::createImageView_() {
  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image                 = m_image_;

  switch (m_desc_.type) {
    case TextureType::Texture1D:
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
      break;
    case TextureType::Texture1DArray:
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
      break;
    case TextureType::Texture2D:
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      break;
    case TextureType::Texture2DArray:
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
      break;
    case TextureType::Texture3D:
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
      break;
    case TextureType::Texture3DArray:
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
      LOG_WARN("3D array textures are not directly supported in Vulkan. Using 3D view instead.");
      break;
    case TextureType::TextureCube:
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
      break;
    default:
      LOG_ERROR("Unsupported texture view type");
      return false;
  }

  viewInfo.format = m_vkFormat_;

  viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  if (g_isDepthFormat(m_desc_.format)) {
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (!g_isDepthOnlyFormat(m_desc_.format)) {
      viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  }

  viewInfo.subresourceRange.baseMipLevel   = 0;
  viewInfo.subresourceRange.levelCount     = m_desc_.mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount     = m_desc_.arraySize;

  if (vkCreateImageView(m_device_->getDevice(), &viewInfo, nullptr, &m_imageView_) != VK_SUCCESS) {
    LOG_ERROR("Failed to create Vulkan image view");
    return false;
  }

  return true;
}

void TextureVk::updateCurrentLayout_(ResourceLayout layout) {
  m_currentLayout_ = layout;
}

void TextureVk::update(const void* data, size_t dataSize, uint32_t mipLevel, uint32_t arrayLayer) {
  if (!data || !dataSize) {
    LOG_ERROR("Invalid data or size for texture update");
    return;
  }

  VmaAllocation stagingAllocation = VK_NULL_HANDLE;
  VkBuffer      stagingBuffer     = m_device_->createStagingBuffer(data, dataSize, stagingAllocation);

  if (stagingBuffer == VK_NULL_HANDLE) {
    LOG_ERROR("Failed to create staging buffer for texture update");
    return;
  }

  CommandBufferDesc cmdBufferDesc;
  cmdBufferDesc.primary = true;

  auto commandBuffer = m_device_->createCommandBuffer(cmdBufferDesc);
  if (!commandBuffer) {
    vmaDestroyBuffer(m_device_->getAllocator(), stagingBuffer, stagingAllocation);
    LOG_ERROR("Failed to create command buffer for texture update");
    return;
  }

  auto initialLayout = m_currentLayout_;

  CommandBufferVk* cmdBufferVk = static_cast<CommandBufferVk*>(commandBuffer.get());

  cmdBufferVk->reset();

  cmdBufferVk->begin();

  ResourceBarrierDesc barrier = {};
  barrier.texture             = this;
  barrier.oldLayout           = initialLayout;
  barrier.newLayout           = ResourceLayout::TransferDst;
  cmdBufferVk->resourceBarrier(barrier);

  uint32_t mipWidth = m_desc_.width >> mipLevel;
  mipWidth          = mipWidth > 0 ? mipWidth : 1;

  uint32_t mipHeight = m_desc_.height >> mipLevel;
  mipHeight          = mipHeight > 0 ? mipHeight : 1;

  uint32_t mipDepth = m_desc_.depth >> mipLevel;
  mipDepth          = mipDepth > 0 ? mipDepth : 1;

  VkBufferImageCopy region = {};
  region.bufferOffset      = 0;
  region.bufferRowLength   = 0;  // Tightly packed
  region.bufferImageHeight = 0;  // Tightly packed

  region.imageSubresource.aspectMask
      = g_isDepthFormat(m_desc_.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel       = mipLevel;
  region.imageSubresource.baseArrayLayer = arrayLayer;
  region.imageSubresource.layerCount     = 1;

  region.imageOffset = {0, 0, 0};
  region.imageExtent = {mipWidth, mipHeight, mipDepth};

  vkCmdCopyBufferToImage(
      cmdBufferVk->getCommandBuffer(), stagingBuffer, m_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  barrier.oldLayout = ResourceLayout::TransferDst;
  barrier.newLayout = (initialLayout == ResourceLayout::Undefined) ? ResourceLayout::General : initialLayout;
  cmdBufferVk->resourceBarrier(barrier);

  cmdBufferVk->end();

  FenceDesc fenceDesc;
  fenceDesc.signaled = false;
  auto fence         = m_device_->createFence(fenceDesc);

  m_device_->submitCommandBuffer(commandBuffer.get(), fence.get());

  fence->wait();

  vmaDestroyBuffer(m_device_->getAllocator(), stagingBuffer, stagingAllocation);
}
}  // namespace rhi
}  // namespace gfx
}  // namespace arise