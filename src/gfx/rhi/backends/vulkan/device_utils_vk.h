#ifndef ARISE_DEVICE_UTILS_VK_H
#define ARISE_DEVICE_UTILS_VK_H

#include "utils/logger/global_logger.h"

#include <vulkan/vulkan.h>

#include <optional>
#include <string>
#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;
  std::optional<uint32_t> computeFamily;
  std::optional<uint32_t> transferFamily;

  bool isComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value();
  }
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR        capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR>   presentModes;
};

VKAPI_ATTR VkBool32 VKAPI_CALL g_debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                               VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                               void*                                       pUserData);

void g_populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

QueueFamilyIndices g_findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

bool g_isDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions);

bool g_isDeviceSuitable(VkPhysicalDevice                device,
                        VkSurfaceKHR                    surface,
                        const std::vector<const char*>& deviceExtensions);

SwapChainSupportDetails g_querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_DEVICE_UTILS_VK_H