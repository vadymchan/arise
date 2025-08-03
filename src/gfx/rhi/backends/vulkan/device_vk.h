#ifndef ARISE_DEVICE_VK_H
#define ARISE_DEVICE_VK_H

#include "gfx/rhi/backends/vulkan/command_buffer_vk.h"
#include "gfx/rhi/backends/vulkan/descriptor_vk.h"
#include "gfx/rhi/backends/vulkan/device_utils_vk.h"
#include "gfx/rhi/interface/device.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

class DeviceVk : public Device {
  public:
  DeviceVk(const DeviceDesc& desc);
  ~DeviceVk() override;

  RenderingApi getApiType() const override { return RenderingApi::Vulkan; }

  // clang-format off
  std::unique_ptr<Buffer>              createBuffer(const BufferDesc& desc) override;
  std::unique_ptr<Texture>             createTexture(const TextureDesc& desc) override;
  std::unique_ptr<Sampler>             createSampler(const SamplerDesc& desc) override;
  std::unique_ptr<Shader>              createShader(const ShaderDesc& desc) override;
  std::unique_ptr<GraphicsPipeline>    createGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
  std::unique_ptr<GraphicsPipeline>    createGraphicsPipelineWithReflection(const GraphicsPipelineDesc& desc) override;
  std::unique_ptr<DescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) override;
  std::unique_ptr<DescriptorSet>       createDescriptorSet(const DescriptorSetLayout* layout) override;
  std::unique_ptr<RenderPass>          createRenderPass(const RenderPassDesc& desc) override;
  std::unique_ptr<Framebuffer>         createFramebuffer(const FramebufferDesc& desc) override;
  std::unique_ptr<CommandBuffer>       createCommandBuffer(const CommandBufferDesc& desc = CommandBufferDesc()) override;
  std::unique_ptr<Fence>               createFence(const FenceDesc& desc = FenceDesc()) override;
  std::unique_ptr<Semaphore>           createSemaphore() override;
  std::unique_ptr<SwapChain>           createSwapChain(const SwapchainDesc& desc) override;
  // clang-format on

  void updateBuffer(Buffer* buffer, const void* data, size_t size, size_t offset = 0) override;
  void updateTexture(
      Texture* texture, const void* data, size_t dataSize, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) override;

  /**
   * The command buffer must already be in the "closed" state (end() - vkEndCommandBuffer must have been called)
   */
  void submitCommandBuffer(CommandBuffer*                 cmdBuffer,
                           Fence*                         signalFence      = nullptr,
                           const std::vector<Semaphore*>& waitSemaphores   = {},
                           const std::vector<Semaphore*>& signalSemaphores = {}) override;

  void waitIdle() override;

  VkInstance                        getInstance() const { return m_instance_; }
  VkPhysicalDevice                  getPhysicalDevice() const { return m_physicalDevice_; }
  VkDevice                          getDevice() const { return m_device_; }
  VmaAllocator                      getAllocator() const { return m_allocator_; }
  VkSurfaceKHR                      getSurface() const { return m_surface_; }
  VkQueue                           getGraphicsQueue() const { return m_graphicsQueue_; }
  VkQueue                           getPresentQueue() const { return m_presentQueue_; }
  VkQueue                           getComputeQueue() const { return m_computeQueue_; }
  QueueFamilyIndices                getQueueFamilyIndices() const { return m_queueFamilyIndices_; }
  const VkPhysicalDeviceProperties& getPhysicalDeviceProperties() const { return m_deviceProperties_; }
  const VkPhysicalDeviceFeatures&   getPhysicalDeviceFeatures() const { return m_deviceFeatures_; }

  std::mutex& getQueueMutex() { return m_queueSubmitMutex; }

  CommandPoolManager&    getCommandPoolManager() { return m_commandPoolManager_; }
  DescriptorPoolManager& getDescriptorPoolManager() { return m_descriptorPoolManager_; }

  VkBuffer createStagingBuffer(const void* data, size_t size, VmaAllocation& allocation);

  private:
  bool createInstance_();
  bool setupDebugMessenger_();
  bool createSurface_();
  bool pickPhysicalDevice_();
  bool createLogicalDevice_();
  bool createCommandPools_();
  bool createDescriptorPools_();
  bool createAllocator_();

  VkInstance               m_instance_       = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT m_debugMessenger_ = VK_NULL_HANDLE;
  VkSurfaceKHR             m_surface_        = VK_NULL_HANDLE;

  VkPhysicalDevice           m_physicalDevice_ = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties m_deviceProperties_{};
  VkPhysicalDeviceFeatures   m_deviceFeatures_{};
  QueueFamilyIndices         m_queueFamilyIndices_;

  VkDevice   m_device_        = VK_NULL_HANDLE;
  VkQueue    m_graphicsQueue_ = VK_NULL_HANDLE;
  VkQueue    m_presentQueue_  = VK_NULL_HANDLE;
  VkQueue    m_computeQueue_  = VK_NULL_HANDLE;
  std::mutex m_queueSubmitMutex;

  VmaAllocator m_allocator_ = VK_NULL_HANDLE;

  // Resource management
  CommandPoolManager    m_commandPoolManager_;
  DescriptorPoolManager m_descriptorPoolManager_;

  // Validation layers
  std::vector<const char*> m_validationLayers_;
  std::vector<const char*> m_deviceExtensions_;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_DEVICE_VK_H