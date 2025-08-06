#include "gfx/rhi/backends/vulkan/synchronization_vk.h"

#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

FenceVk::FenceVk(const FenceDesc& desc, DeviceVk* device)
    : Fence(desc)
    , m_device_(device) {
  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags             = desc.signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

  if (vkCreateFence(device->getDevice(), &fenceInfo, nullptr, &m_fence_) != VK_SUCCESS) {
    LOG_ERROR("Failed to create Vulkan fence");
  }
}

FenceVk::~FenceVk() {
  if (m_device_ && m_fence_ != VK_NULL_HANDLE) {
    vkDestroyFence(m_device_->getDevice(), m_fence_, nullptr);
    m_fence_ = VK_NULL_HANDLE;
  }
}

void FenceVk::reset() {
  if (m_fence_ != VK_NULL_HANDLE) {
    vkResetFences(m_device_->getDevice(), 1, &m_fence_);
    m_signaled_ = false;
  }
}

bool FenceVk::wait(uint64_t timeout) {
  if (m_fence_ == VK_NULL_HANDLE) {
    return false;
  }

  VkResult result = vkWaitForFences(m_device_->getDevice(), 1, &m_fence_, VK_TRUE, timeout);
  return result == VK_SUCCESS;
}

bool FenceVk::isSignaled() {
  if (m_fence_ == VK_NULL_HANDLE) {
    return false;
  }

  VkResult result = vkGetFenceStatus(m_device_->getDevice(), m_fence_);
  m_signaled_     = (result == VK_SUCCESS);
  return m_signaled_;
}

void FenceVk::signal(VkQueue queue) {
  VkSubmitInfo submitInfo       = {};
  submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 0;

  {
    std::lock_guard<std::mutex> lock(m_device_->getQueueMutex());
    if (vkQueueSubmit(queue, 1, &submitInfo, m_fence_) != VK_SUCCESS) {
      LOG_ERROR("Failed to signal Vulkan fence");
    } else {
      m_signaled_ = true;
    }
  }
}

SemaphoreVk::SemaphoreVk(DeviceVk* device)
    : m_device_(device) {
  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  if (vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &m_semaphore_) != VK_SUCCESS) {
    LOG_ERROR("Failed to create Vulkan semaphore");
  }
}

SemaphoreVk::~SemaphoreVk() {
  if (m_device_ && m_semaphore_ != VK_NULL_HANDLE) {
    vkDestroySemaphore(m_device_->getDevice(), m_semaphore_, nullptr);
  }
}

void SemaphoreVk::signal(VkQueue queue) {
  VkSubmitInfo submitInfo         = {};
  submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount   = 0;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores    = &m_semaphore_;

  {
    std::lock_guard<std::mutex> lock(m_device_->getQueueMutex());
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
      LOG_ERROR("Failed to signal Vulkan semaphore");
    }
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise