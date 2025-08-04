#ifndef ARISE_SWAP_CHAIN_VK_H
#define ARISE_SWAP_CHAIN_VK_H

#include "gfx/rhi/backends/vulkan/device_utils_vk.h"
#include "gfx/rhi/interface/swap_chain.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

class DeviceVk;
class TextureVk;

class SwapChainVk : public SwapChain {
  public:
  SwapChainVk(const SwapchainDesc& desc, DeviceVk* device);
  ~SwapChainVk() override;

  Texture* getCurrentImage() override;
  bool     acquireNextImage(Semaphore* signalSemaphore = nullptr) override;
  bool     present(Semaphore* waitSemaphore = nullptr) override;
  bool     resize(uint32_t width, uint32_t height) override;

  TextureFormat getFormat() const override;
  uint32_t      getWidth() const override;
  uint32_t      getHeight() const override;
  uint32_t      getBufferCount() const override;

  // Vulkan-specific methods
  VkSwapchainKHR getNativeSwapChain() const { return m_swapChain_; }

  private:
  void               cleanup_();
  bool               createSwapChain_();
  bool               createImageViews_();
  VkSurfaceFormatKHR chooseSurfaceFormat_(const std::vector<VkSurfaceFormatKHR>& availableFormats);
  VkPresentModeKHR   choosePresentMode_(const std::vector<VkPresentModeKHR>& availablePresentModes);
  VkExtent2D         chooseSwapExtent_(const VkSurfaceCapabilitiesKHR& capabilities);

  private:
  DeviceVk* m_device_;

  VkSwapchainKHR     m_swapChain_     = VK_NULL_HANDLE;
  VkFormat           m_depthFormat_   = VK_FORMAT_D32_SFLOAT;
  VkSurfaceFormatKHR m_surfaceFormat_ = {};
  VkExtent2D         m_extent_        = {};

  std::vector<VkImage>                    m_images_;
  std::vector<VkImageView>                m_imageViews_;
  std::vector<std::unique_ptr<TextureVk>> m_textures_;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_SWAP_CHAIN_VK_H