#include "gfx/rhi/backends/vulkan/swap_chain_vk.h"

#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"
#include "gfx/rhi/backends/vulkan/synchronization_vk.h"
#include "gfx/rhi/backends/vulkan/texture_vk.h"
#include "utils/logger/log.h"

#include <algorithm>

namespace arise {
namespace gfx {
namespace rhi {

SwapChainVk::SwapChainVk(const SwapchainDesc& desc, DeviceVk* device)
    : SwapChain(desc)
    , m_device_(device) {
  if (!createSwapChain_()) {
    LOG_ERROR("Failed to create swap chain");
    return;
  }

  if (!createImageViews_()) {
    LOG_ERROR("Failed to create swap chain image views");
    return;
  }
}

SwapChainVk::~SwapChainVk() {
  cleanup_();
}

void SwapChainVk::cleanup_() {
  VkDevice device = m_device_->getDevice();

  m_textures_.clear();

  for (auto& imageView : m_imageViews_) {
    vkDestroyImageView(device, imageView, nullptr);
  }
  m_imageViews_.clear();

  if (m_swapChain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, m_swapChain_, nullptr);
    m_swapChain_ = VK_NULL_HANDLE;
  }

  m_images_.clear();
}

bool SwapChainVk::createSwapChain_() {
  SwapChainSupportDetails swapChainSupport
      = g_querySwapChainSupport(m_device_->getPhysicalDevice(), m_device_->getSurface());

  VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat_(swapChainSupport.formats);
  VkPresentModeKHR   presentMode   = choosePresentMode_(swapChainSupport.presentModes);
  VkExtent2D         extent        = chooseSwapExtent_(swapChainSupport.capabilities);

  m_surfaceFormat_ = surfaceFormat;
  m_extent_        = extent;

  uint32_t imageCount = m_desc_.bufferCount;

  if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  if (imageCount < swapChainSupport.capabilities.minImageCount) {
    imageCount = swapChainSupport.capabilities.minImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo = {};
  createInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface                  = m_device_->getSurface();
  createInfo.minImageCount            = imageCount;
  createInfo.imageFormat              = surfaceFormat.format;
  createInfo.imageColorSpace          = surfaceFormat.colorSpace;
  createInfo.imageExtent              = extent;
  createInfo.imageArrayLayers         = 1;
  createInfo.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  QueueFamilyIndices indices              = m_device_->getQueueFamilyIndices();
  uint32_t           queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

  if (indices.graphicsFamily.value() != indices.presentFamily.value()) {
    createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices   = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices   = nullptr;
  }

  createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode    = presentMode;
  createInfo.clipped        = VK_TRUE;
  createInfo.oldSwapchain   = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(m_device_->getDevice(), &createInfo, nullptr, &m_swapChain_) != VK_SUCCESS) {
    return false;
  }

  uint32_t actualImageCount;
  vkGetSwapchainImagesKHR(m_device_->getDevice(), m_swapChain_, &actualImageCount, nullptr);
  m_images_.resize(actualImageCount);
  vkGetSwapchainImagesKHR(m_device_->getDevice(), m_swapChain_, &actualImageCount, m_images_.data());

  return true;
}

bool SwapChainVk::createImageViews_() {
  m_imageViews_.resize(m_images_.size());
  m_textures_.resize(m_images_.size());

  for (size_t i = 0; i < m_images_.size(); i++) {
    VkImageViewCreateInfo viewInfo           = {};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_images_[i];
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = m_surfaceFormat_.format;
    viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(m_device_->getDevice(), &viewInfo, nullptr, &m_imageViews_[i]) != VK_SUCCESS) {
      return false;
    }

    TextureDesc textureDesc;
    textureDesc.type          = TextureType::Texture2D;
    textureDesc.format        = getFormat();
    textureDesc.width         = m_extent_.width;
    textureDesc.height        = m_extent_.height;
    textureDesc.depth         = 1;
    textureDesc.mipLevels     = 1;
    textureDesc.arraySize     = 1;
    textureDesc.sampleCount   = MSAASamples::Count1;
    textureDesc.createFlags   = TextureCreateFlag::Rtv;
    textureDesc.initialLayout = ResourceLayout::Undefined;
    textureDesc.debugName     = "back_buffer_texture";

    m_textures_[i] = std::make_unique<TextureVk>(m_device_, textureDesc, m_images_[i], m_imageViews_[i]);
  }

  CommandBufferDesc cmdBufDesc;
  auto              cmdBuffer = m_device_->createCommandBuffer(cmdBufDesc);
  cmdBuffer->begin();

  for (auto& texture : m_textures_) {
    ResourceBarrierDesc barrier;
    barrier.texture   = texture.get();
    barrier.oldLayout = ResourceLayout::Undefined;
    barrier.newLayout = ResourceLayout::PresentSrc;
    cmdBuffer->resourceBarrier(barrier);
  }

  cmdBuffer->end();

  FenceDesc fenceDesc;
  auto      fence = m_device_->createFence(fenceDesc);
  m_device_->submitCommandBuffer(cmdBuffer.get(), fence.get());
  fence->wait();

  return true;
}

VkSurfaceFormatKHR SwapChainVk::chooseSurfaceFormat_(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
  VkFormat preferredFormat = g_getTextureFormatVk(m_desc_.format);

  // First, try to find the preferred format (usually BGRA8 or RGBA8)
  for (const auto& format : availableFormats) {
    if (format.format == preferredFormat && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }

  // If we can't find our preferred format, look for BGRA8 or RGBA8 with sRGB color space
  for (const auto& format : availableFormats) {
    if ((format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_R8G8B8A8_SRGB)
        && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }

  return availableFormats[0];
}

VkPresentModeKHR SwapChainVk::choosePresentMode_(const std::vector<VkPresentModeKHR>& availablePresentModes) {
  // Always prefer IMMEDIATE mode for simplest behavior (no vsync for - for simplicity)
  for (const auto& presentMode : availablePresentModes) {
    if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
      return presentMode;
    }
  }

  // Next preference is MAILBOX if available
  for (const auto& presentMode : availablePresentModes) {
    if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return presentMode;
    }
  }

  // Fall back to FIFO which is guaranteed to be available
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChainVk::chooseSwapExtent_(const VkSurfaceCapabilitiesKHR& capabilities) {
  if (capabilities.currentExtent.width != UINT32_MAX) {
    return capabilities.currentExtent;
  } else {
    VkExtent2D actualExtent = {m_desc_.width, m_desc_.height};

    actualExtent.width
        = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height
        = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

Texture* SwapChainVk::getCurrentImage() {
  return m_textures_[m_currentFrameIndex].get();
}

bool SwapChainVk::acquireNextImage(Semaphore* signalSemaphore) {
  VkSemaphore vkSemaphore = VK_NULL_HANDLE;
  if (signalSemaphore) {
    SemaphoreVk* semaphoreVk = dynamic_cast<SemaphoreVk*>(signalSemaphore);
    if (semaphoreVk) {
      vkSemaphore = semaphoreVk->getSemaphore();
    }
  }

  uint64_t timeoutOneSecond = 1'000'000'000;
  VkResult result           = vkAcquireNextImageKHR(
      m_device_->getDevice(), m_swapChain_, timeoutOneSecond, vkSemaphore, VK_NULL_HANDLE, &m_currentFrameIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    resize(m_desc_.width, m_desc_.height);
    return false;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    LOG_ERROR("Failed to acquire swap chain image");
    return false;
  }

  return true;
}

bool SwapChainVk::present(Semaphore* waitSemaphore) {
  std::vector<VkSemaphore> waitSemaphores;
  if (waitSemaphore) {
    SemaphoreVk* semaphoreVk = dynamic_cast<SemaphoreVk*>(waitSemaphore);
    if (semaphoreVk) {
      waitSemaphores.push_back(semaphoreVk->getSemaphore());
    }
  }

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType            = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  if (!waitSemaphores.empty()) {
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    presentInfo.pWaitSemaphores    = waitSemaphores.data();
  }

  VkSwapchainKHR vkSwapChain = m_swapChain_;
  uint32_t       imageIndex  = m_currentFrameIndex;

  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains    = &vkSwapChain;
  presentInfo.pImageIndices  = &imageIndex;

  VkResult result;
  {
    // TODO: temp fix (this reduces threading issue with main thread and worker thread (for asset loading))
    std::lock_guard<std::mutex> lock(m_device_->getQueueMutex());
    result = vkQueuePresentKHR(m_device_->getPresentQueue(), &presentInfo);
  }

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    resize(m_desc_.width, m_desc_.height);
  } else if (result != VK_SUCCESS) {
    LOG_ERROR("Failed to present swap chain image");
    return false;
  }

  return true;
}

bool SwapChainVk::resize(uint32_t width, uint32_t height) {
  if (width == m_extent_.width && height == m_extent_.height) {
    return true;
  }

  m_device_->waitIdle();

  m_desc_.width  = width;
  m_desc_.height = height;

  cleanup_();

  if (!createSwapChain_() || !createImageViews_()) {
    LOG_ERROR("Failed to recreate swap chain during resize");
    return false;
  }

  return true;
}

TextureFormat SwapChainVk::getFormat() const {
  return g_getTextureFormatVk(m_surfaceFormat_.format);
}

uint32_t SwapChainVk::getWidth() const {
  return m_extent_.width;
}

uint32_t SwapChainVk::getHeight() const {
  return m_extent_.height;
}

uint32_t SwapChainVk::getBufferCount() const {
  return static_cast<uint32_t>(m_images_.size());
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise