#include "gfx/rhi/backends/vulkan/device_vk.h"

#include "gfx/rhi/backends/vulkan/buffer_vk.h"
#include "gfx/rhi/backends/vulkan/command_buffer_vk.h"
#include "gfx/rhi/backends/vulkan/descriptor_vk.h"
#include "gfx/rhi/backends/vulkan/framebuffer_vk.h"
#include "gfx/rhi/backends/vulkan/pipeline_vk.h"
#include "gfx/rhi/backends/vulkan/render_pass_vk.h"
#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"
#include "gfx/rhi/backends/vulkan/sampler_vk.h"
#include "gfx/rhi/backends/vulkan/shader_vk.h"
#include "gfx/rhi/backends/vulkan/swap_chain_vk.h"
#include "gfx/rhi/backends/vulkan/synchronization_vk.h"
#include "gfx/rhi/backends/vulkan/texture_vk.h"
#include "gfx/rhi/shader_reflection/pipeline_utils.h"
#include "platform/common/window.h"
#include "profiler/backends/gpu_profiler.h"
#include "utils/logger/global_logger.h"
#include "utils/service/service_locator.h"

#include <SDL_vulkan.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <map>
#include <set>

namespace arise {
namespace gfx {
namespace rhi {

constexpr uint32_t DESCRIPTOR_POOL_MAX_SETS = 1000;

//-------------------------------------------------------------------------
// DeviceVk implementation
//-------------------------------------------------------------------------

DeviceVk::DeviceVk(const DeviceDesc& desc)
    : Device(desc) {
#ifdef _DEBUG
  m_validationLayers_ = {"VK_LAYER_KHRONOS_validation"};
#endif
  m_deviceExtensions_ = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  if (!createInstance_() || !setupDebugMessenger_() || !createSurface_() || !pickPhysicalDevice_()
      || !createLogicalDevice_() || !createAllocator_() || !createCommandPools_() || !createDescriptorPools_()) {
    // Handle initialization failure:
    // - add logger
    // - make proper error handling and cleanup
  }
}

DeviceVk::~DeviceVk() {
  waitIdle();

  m_descriptorPoolManager_.release();
  m_commandPoolManager_.release();

  if (m_allocator_) {
    vmaDestroyAllocator(m_allocator_);
    m_allocator_ = VK_NULL_HANDLE;
  }

  if (m_device_) {
    vkDestroyDevice(m_device_, nullptr);
  }

  if (m_instance_ && m_surface_) {
    vkDestroySurfaceKHR(m_instance_, m_surface_, nullptr);
  }

#ifdef _DEBUG
  if (m_instance_ && m_debugMessenger_) {
    auto vkDestroyDebugUtilsMessengerEXT
        = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance_, "vkDestroyDebugUtilsMessengerEXT");

    if (vkDestroyDebugUtilsMessengerEXT) {
      vkDestroyDebugUtilsMessengerEXT(m_instance_, m_debugMessenger_, nullptr);
    }
  }
#endif

  if (m_instance_) {
    vkDestroyInstance(m_instance_, nullptr);
  }
}

bool DeviceVk::createInstance_() {
  VkApplicationInfo appInfo  = {};
  appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName   = "arise";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName        = "arise";
  appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion         = VK_API_VERSION_1_3;  // TODO: consider switching to 1.4

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo     = &appInfo;

#ifdef _DEBUG
  createInfo.enabledLayerCount   = static_cast<uint32_t>(m_validationLayers_.size());
  createInfo.ppEnabledLayerNames = m_validationLayers_.data();

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
  g_populateDebugMessengerCreateInfo(debugCreateInfo);

#if defined(ARISE_USE_VULKAN_BEST_PRACTICES_VALIDATION)
  VkValidationFeatureEnableEXT validationFeatures[] = {VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT};

  VkValidationFeaturesEXT validationFeaturesExt       = {};
  validationFeaturesExt.sType                         = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
  validationFeaturesExt.enabledValidationFeatureCount = 1;
  validationFeaturesExt.pEnabledValidationFeatures    = validationFeatures;
  validationFeaturesExt.pNext                         = &debugCreateInfo;
  createInfo.pNext                                    = &validationFeaturesExt;
#else
  createInfo.pNext = &debugCreateInfo;
#endif
#else
  createInfo.enabledLayerCount = 0;
  createInfo.pNext             = nullptr;
#endif

  std::vector<const char*> extensions;

  unsigned int sdlExtensionCount = 0;
  if (!SDL_Vulkan_GetInstanceExtensions(
          static_cast<SDL_Window*>(getWindow()->getNativeWindowHandle()), &sdlExtensionCount, nullptr)) {
    return false;
  }

  extensions.resize(sdlExtensionCount);
  if (!SDL_Vulkan_GetInstanceExtensions(
          static_cast<SDL_Window*>(getWindow()->getNativeWindowHandle()), &sdlExtensionCount, extensions.data())) {
    return false;
  }

#ifdef _DEBUG
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  if (vkCreateInstance(&createInfo, nullptr, &m_instance_) != VK_SUCCESS) {
    return false;
  }

  return true;
}

bool DeviceVk::setupDebugMessenger_() {
#ifdef _DEBUG
  if (m_validationLayers_.empty()) {
    return true;
  }

  VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
  g_populateDebugMessengerCreateInfo(createInfo);

  auto vkCreateDebugUtilsMessengerEXT
      = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance_, "vkCreateDebugUtilsMessengerEXT");

  if (!vkCreateDebugUtilsMessengerEXT) {
    return false;
  }

  if (vkCreateDebugUtilsMessengerEXT(m_instance_, &createInfo, nullptr, &m_debugMessenger_) != VK_SUCCESS) {
    return false;
  }
#endif

  return true;
}

bool DeviceVk::createSurface_() {
  if (!SDL_Vulkan_CreateSurface(
          static_cast<SDL_Window*>(getWindow()->getNativeWindowHandle()), m_instance_, &m_surface_)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create Vulkan surface: " + std::string(SDL_GetError()));
    return false;
  }

  return true;
}

bool DeviceVk::pickPhysicalDevice_() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(m_instance_, &deviceCount, nullptr);

  if (deviceCount == 0) {
    GlobalLogger::Log(LogLevel::Error, "Failed to find GPUs with Vulkan support");
    return false;
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(m_instance_, &deviceCount, devices.data());

  // Rate devices (based on properties) and pick the best one
  std::multimap<int, VkPhysicalDevice> candidates;

  for (const auto& device : devices) {
    if (!g_isDeviceSuitable(device, m_surface_, m_deviceExtensions_)) {
      continue;
    }

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    int score = 0;

    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      score += 1000;
    } else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
      score += 100;
    }

    score += deviceProperties.limits.maxImageDimension2D / 1024;

    candidates.insert(std::make_pair(score, device));
  }

  if (candidates.empty()) {
    return false;
  }

  m_physicalDevice_ = candidates.rbegin()->second;

  vkGetPhysicalDeviceProperties(m_physicalDevice_, &m_deviceProperties_);
  vkGetPhysicalDeviceFeatures(m_physicalDevice_, &m_deviceFeatures_);

  m_queueFamilyIndices_ = g_findQueueFamilies(m_physicalDevice_, m_surface_);

  return true;
}

bool DeviceVk::createLogicalDevice_() {
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t>                   uniqueQueueFamilies = {m_queueFamilyIndices_.graphicsFamily.value(),
                                                              m_queueFamilyIndices_.presentFamily.value(),
                                                              m_queueFamilyIndices_.computeFamily.value()};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex        = queueFamily;
    queueCreateInfo.queueCount              = 1;
    queueCreateInfo.pQueuePriorities        = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures = {};
  deviceFeatures.samplerAnisotropy        = VK_TRUE;
  deviceFeatures.fillModeNonSolid         = VK_TRUE;
  deviceFeatures.geometryShader           = VK_TRUE;

  VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {};
  descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
  descriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
  descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind  = VK_TRUE;
  descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
  descriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending     = VK_TRUE;
  descriptorIndexingFeatures.descriptorBindingPartiallyBound               = VK_TRUE;
  descriptorIndexingFeatures.runtimeDescriptorArray                        = VK_TRUE;

  VkDeviceCreateInfo createInfo      = {};
  createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pNext                   = &descriptorIndexingFeatures;
  createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos       = queueCreateInfos.data();
  createInfo.pEnabledFeatures        = &deviceFeatures;
  createInfo.enabledExtensionCount   = static_cast<uint32_t>(m_deviceExtensions_.size());
  createInfo.ppEnabledExtensionNames = m_deviceExtensions_.data();

#ifdef _DEBUG
  createInfo.enabledLayerCount   = static_cast<uint32_t>(m_validationLayers_.size());
  createInfo.ppEnabledLayerNames = m_validationLayers_.data();
#else
  createInfo.enabledLayerCount = 0;
#endif

  if (vkCreateDevice(m_physicalDevice_, &createInfo, nullptr, &m_device_) != VK_SUCCESS) {
    return false;
  }

  vkGetDeviceQueue(m_device_, m_queueFamilyIndices_.graphicsFamily.value(), 0, &m_graphicsQueue_);
  vkGetDeviceQueue(m_device_, m_queueFamilyIndices_.presentFamily.value(), 0, &m_presentQueue_);
  vkGetDeviceQueue(m_device_, m_queueFamilyIndices_.computeFamily.value(), 0, &m_computeQueue_);

  return true;
}

bool DeviceVk::createCommandPools_() {
  return m_commandPoolManager_.initialize(m_device_, m_queueFamilyIndices_.graphicsFamily.value());
}

bool DeviceVk::createDescriptorPools_() {
  return m_descriptorPoolManager_.initialize(m_device_, DESCRIPTOR_POOL_MAX_SETS);
}

bool DeviceVk::createAllocator_() {
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice         = m_physicalDevice_;
  allocatorInfo.device                 = m_device_;
  allocatorInfo.instance               = m_instance_;
  allocatorInfo.flags                  = 0;

  VkResult result = vmaCreateAllocator(&allocatorInfo, &m_allocator_);
  if (result != VK_SUCCESS) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create Vulkan Memory Allocator");
    return false;
  }

  return true;
}

VkBuffer DeviceVk::createStagingBuffer(const void* data, size_t size, VmaAllocation& allocation) {
  VkBuffer stagingBuffer;

  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size               = size;
  bufferInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage                   = VMA_MEMORY_USAGE_CPU_TO_GPU;
  allocInfo.flags                   = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VmaAllocationInfo allocationInfo;
  VkResult          result
      = vmaCreateBuffer(m_allocator_, &bufferInfo, &allocInfo, &stagingBuffer, &allocation, &allocationInfo);

  if (result != VK_SUCCESS) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create staging buffer with VMA");
    return VK_NULL_HANDLE;
  }

  memcpy(allocationInfo.pMappedData, data, size);

  return stagingBuffer;
}

std::unique_ptr<Buffer> DeviceVk::createBuffer(const BufferDesc& desc) {
  return std::make_unique<BufferVk>(desc, this);
}

std::unique_ptr<Texture> DeviceVk::createTexture(const TextureDesc& desc) {
  auto texture   = std::make_unique<TextureVk>(desc, this);
  auto textureVk = static_cast<TextureVk*>(texture.get());

  if (desc.initialLayout != ResourceLayout::Undefined) {
    CommandBufferDesc cmdDesc;
    auto              cmdBuffer = createCommandBuffer(cmdDesc);
    cmdBuffer->reset();
    cmdBuffer->begin();
    textureVk->transitionToInitialLayout(cmdBuffer.get());
    cmdBuffer->end();

    auto fence = createFence();
    submitCommandBuffer(cmdBuffer.get(), fence.get());
    fence->wait();
  }

  return texture;
}

std::unique_ptr<Sampler> DeviceVk::createSampler(const SamplerDesc& desc) {
  return std::make_unique<SamplerVk>(desc, this);
}

std::unique_ptr<Shader> DeviceVk::createShader(const ShaderDesc& desc) {
  return std::make_unique<ShaderVk>(desc, this);
}

std::unique_ptr<GraphicsPipeline> DeviceVk::createGraphicsPipeline(const GraphicsPipelineDesc& desc) {
  return std::make_unique<GraphicsPipelineVk>(desc, this);
}

std::unique_ptr<GraphicsPipeline> DeviceVk::createGraphicsPipelineWithReflection(const GraphicsPipelineDesc& desc) {
  PipelineLayoutDesc reflectionLayout = pipeline_utils::generatePipelineLayoutFromShaders(desc.shaders);

  std::vector<std::unique_ptr<DescriptorSetLayout>> ownedLayouts;
  auto layoutPtrs = pipeline_utils::createDescriptorSetLayouts(this, reflectionLayout, ownedLayouts);

  GraphicsPipelineDesc modifiedDesc = desc;
  modifiedDesc.setLayouts           = layoutPtrs;

  auto pipeline = std::make_unique<GraphicsPipelineVk>(modifiedDesc, this);

  return std::move(pipeline);
}

std::unique_ptr<DescriptorSetLayout> DeviceVk::createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) {
  return std::make_unique<DescriptorSetLayoutVk>(desc, this);
}

std::unique_ptr<DescriptorSet> DeviceVk::createDescriptorSet(const DescriptorSetLayout* layout) {
  const DescriptorSetLayoutVk* descriptorSetLayoutVk = dynamic_cast<const DescriptorSetLayoutVk*>(layout);
  if (!descriptorSetLayoutVk) {
    return nullptr;
  }

  return std::make_unique<DescriptorSetVk>(this, descriptorSetLayoutVk);
}

std::unique_ptr<RenderPass> DeviceVk::createRenderPass(const RenderPassDesc& desc) {
  return std::make_unique<RenderPassVk>(desc, this);
}

std::unique_ptr<Framebuffer> DeviceVk::createFramebuffer(const FramebufferDesc& desc) {
  return std::make_unique<FramebufferVk>(desc, this);
}

std::unique_ptr<CommandBuffer> DeviceVk::createCommandBuffer(const CommandBufferDesc& desc) {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool                 = m_commandPoolManager_.getPool();
  allocInfo.level              = desc.primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  if (vkAllocateCommandBuffers(m_device_, &allocInfo, &commandBuffer) != VK_SUCCESS) {
    return nullptr;
  }

  return std::make_unique<CommandBufferVk>(this, commandBuffer, allocInfo.commandPool);
}

std::unique_ptr<Fence> DeviceVk::createFence(const FenceDesc& desc) {
  return std::make_unique<FenceVk>(desc, this);
}

std::unique_ptr<Semaphore> DeviceVk::createSemaphore() {
  return std::make_unique<SemaphoreVk>(this);
}

std::unique_ptr<SwapChain> DeviceVk::createSwapChain(const SwapchainDesc& desc) {
  return std::make_unique<SwapChainVk>(desc, this);
}

void DeviceVk::updateBuffer(Buffer* buffer, const void* data, size_t size, size_t offset) {
  BufferVk* bufferVk = dynamic_cast<BufferVk*>(buffer);
  if (!bufferVk) {
    GlobalLogger::Log(LogLevel::Error, "Invalid buffer type");
    return;
  }

  if (!data || size == 0) {
    GlobalLogger::Log(LogLevel::Warning, "No data to update");
    return;
  }

  if (offset + size > bufferVk->getSize()) {
    GlobalLogger::Log(LogLevel::Error, "Update exceeds buffer size");
    return;
  }

  if (bufferVk->isMapped()) {
    if (bufferVk->update_(data, size, offset)) {
      return;
    }
  }

  VmaAllocation stagingAllocation = VK_NULL_HANDLE;
  VkBuffer      stagingBuffer     = createStagingBuffer(data, size, stagingAllocation);

  if (stagingBuffer == VK_NULL_HANDLE) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create staging buffer");
    return;
  }

  CommandBufferDesc cmdBufferDesc;
  cmdBufferDesc.primary = true;
  auto cmdBuffer        = createCommandBuffer(cmdBufferDesc);

  cmdBuffer->begin();

  VkBufferCopy copyRegion = {};
  copyRegion.srcOffset    = 0;
  copyRegion.dstOffset    = offset;
  copyRegion.size         = size;

  vkCmdCopyBuffer(static_cast<CommandBufferVk*>(cmdBuffer.get())->getCommandBuffer(),
                  stagingBuffer,
                  bufferVk->getBuffer(),
                  1,
                  &copyRegion);

  cmdBuffer->end();

  FenceDesc fenceDesc;
  auto      fence = createFence(fenceDesc);
  submitCommandBuffer(cmdBuffer.get(), fence.get());
  fence->wait();

  vmaDestroyBuffer(m_allocator_, stagingBuffer, stagingAllocation);
}

void DeviceVk::updateTexture(
    Texture* texture, const void* data, size_t dataSize, uint32_t mipLevel, uint32_t arrayLayer) {
  TextureVk* textureVk = dynamic_cast<TextureVk*>(texture);
  if (!textureVk) {
    return;
  }

  textureVk->update(data, dataSize, mipLevel, arrayLayer);
}

void DeviceVk::submitCommandBuffer(CommandBuffer*                 cmdBuffer,
                                   Fence*                         signalFence,
                                   const std::vector<Semaphore*>& waitSemaphores,
                                   const std::vector<Semaphore*>& signalSemaphores) {
  CommandBufferVk* cmdBufferVk = dynamic_cast<CommandBufferVk*>(cmdBuffer);
  if (!cmdBufferVk) {
    return;
  }

  std::vector<VkSemaphore>          waitSemaphoresVk;
  std::vector<VkPipelineStageFlags> waitStages;

  for (Semaphore* semaphore : waitSemaphores) {
    SemaphoreVk* semaphoreVk = dynamic_cast<SemaphoreVk*>(semaphore);
    if (semaphoreVk) {
      waitSemaphoresVk.push_back(semaphoreVk->getSemaphore());
      waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
  }

  std::vector<VkSemaphore> signalSemaphoresVk;
  for (Semaphore* semaphore : signalSemaphores) {
    SemaphoreVk* semaphoreVk = dynamic_cast<SemaphoreVk*>(semaphore);
    if (semaphoreVk) {
      signalSemaphoresVk.push_back(semaphoreVk->getSemaphore());
    }
  }

  VkFence fenceVk = VK_NULL_HANDLE;
  if (signalFence) {
    FenceVk* vkFence_ = dynamic_cast<FenceVk*>(signalFence);
    if (vkFence_) {
      fenceVk = vkFence_->getFence();
    }
  }

  VkSubmitInfo submitInfo       = {};
  submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cmdBufferVk->getCommandBuffer();

  if (!waitSemaphoresVk.empty()) {
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphoresVk.size());
    submitInfo.pWaitSemaphores    = waitSemaphoresVk.data();
    submitInfo.pWaitDstStageMask  = waitStages.data();
  }

  if (!signalSemaphoresVk.empty()) {
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphoresVk.size());
    submitInfo.pSignalSemaphores    = signalSemaphoresVk.data();
  }

  {
    // TODO: I have threading issue here - both main thread and worker thread (for loading assets) are using queue even
    // though there's already mutex here
    std::lock_guard<std::mutex> lock(m_queueSubmitMutex);
    if (vkQueueSubmit(m_graphicsQueue_, 1, &submitInfo, fenceVk) != VK_SUCCESS) {
      GlobalLogger::Log(LogLevel::Error, "Failed to submit command buffer");
    }
  }
}

void DeviceVk::waitIdle() {
  if (m_device_) {
    vkDeviceWaitIdle(m_device_);
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise