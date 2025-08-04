#include "gfx/rhi/backends/vulkan/descriptor_vk.h"

#include "gfx/rhi/backends/vulkan/buffer_vk.h"
#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"
#include "gfx/rhi/backends/vulkan/sampler_vk.h"
#include "gfx/rhi/backends/vulkan/texture_vk.h"
#include "utils/logger/global_logger.h"

namespace arise {
namespace gfx {
namespace rhi {

//-------------------------------------------------------------------------
// DescriptorSetLayoutVk implementation
//-------------------------------------------------------------------------

DescriptorSetLayoutVk::DescriptorSetLayoutVk(const DescriptorSetLayoutDesc& desc, DeviceVk* device)
    : DescriptorSetLayout(desc)
    , m_device_(device) {
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
  layoutBindings.reserve(desc.bindings.size());

  for (const auto& binding : desc.bindings) {
    VkDescriptorSetLayoutBinding vkBinding = {};
    vkBinding.binding                      = binding.binding;
    vkBinding.descriptorType               = g_getShaderBindingTypeVk(binding.type);
    vkBinding.descriptorCount              = binding.descriptorCount;
    vkBinding.stageFlags                   = g_getShaderStageFlagsVk(binding.stageFlags);
    vkBinding.pImmutableSamplers           = nullptr;

    layoutBindings.push_back(vkBinding);
  }

  // Add binding flags to allow updating descriptor sets while in use
  std::vector<VkDescriptorBindingFlags> bindingFlags(layoutBindings.size(),
                                                     VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {};
  bindingFlagsInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  bindingFlagsInfo.bindingCount  = static_cast<uint32_t>(bindingFlags.size());
  bindingFlagsInfo.pBindingFlags = bindingFlags.data();

  VkDescriptorSetLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.pNext                           = &bindingFlagsInfo;
  layoutInfo.bindingCount                    = static_cast<uint32_t>(layoutBindings.size());
  layoutInfo.pBindings                       = layoutBindings.data();
  layoutInfo.flags                           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

  if (vkCreateDescriptorSetLayout(device->getDevice(), &layoutInfo, nullptr, &m_layout_) != VK_SUCCESS) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create Vulkan descriptor set layout");
  }
}

DescriptorSetLayoutVk::~DescriptorSetLayoutVk() {
  if (m_device_ && m_layout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_device_->getDevice(), m_layout_, nullptr);
    m_layout_ = VK_NULL_HANDLE;
  }
}

//-------------------------------------------------------------------------
// DescriptorSetVk implementation
//-------------------------------------------------------------------------

DescriptorSetVk::DescriptorSetVk(DeviceVk* device, const DescriptorSetLayoutVk* layout)
    : m_device_(device)
    , m_layout_(layout) {
  m_descriptorSet_ = device->getDescriptorPoolManager().allocateDescriptorSet(layout->getLayout());

  if (m_descriptorSet_ == VK_NULL_HANDLE) {
    GlobalLogger::Log(LogLevel::Error, "Failed to allocate Vulkan descriptor set");
  }
}

DescriptorSetVk::~DescriptorSetVk() {
  m_descriptorSet_ = VK_NULL_HANDLE;
}

void DescriptorSetVk::setUniformBuffer(uint32_t binding, Buffer* buffer, uint64_t offset, uint64_t range) {
  if (!buffer) {
    GlobalLogger::Log(LogLevel::Error, "Null buffer in setUniformBuffer");
    return;
  }

  BufferVk* bufferVk = dynamic_cast<BufferVk*>(buffer);
  if (!bufferVk) {
    GlobalLogger::Log(LogLevel::Error, "Invalid buffer type in setUniformBuffer");
    return;
  }

  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer                 = bufferVk->getBuffer();
  bufferInfo.offset                 = offset;
  bufferInfo.range                  = (range == 0) ? bufferVk->getSize() - offset : range;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet               = m_descriptorSet_;
  descriptorWrite.dstBinding           = binding;
  descriptorWrite.dstArrayElement      = 0;
  descriptorWrite.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorWrite.descriptorCount      = 1;
  descriptorWrite.pBufferInfo          = &bufferInfo;

  vkUpdateDescriptorSets(m_device_->getDevice(), 1, &descriptorWrite, 0, nullptr);
}

void DescriptorSetVk::setTextureSampler(uint32_t binding, Texture* texture, Sampler* sampler) {
  if (!texture || !sampler) {
    GlobalLogger::Log(LogLevel::Error, "Null texture or sampler in setTextureSampler");
    return;
  }

  TextureVk* textureVk = dynamic_cast<TextureVk*>(texture);
  SamplerVk* samplerVk = dynamic_cast<SamplerVk*>(sampler);

  if (!textureVk || !samplerVk) {
    GlobalLogger::Log(LogLevel::Error, "Invalid texture or sampler type in setTextureSampler");
    return;
  }

  VkDescriptorImageInfo imageInfo = {};
  imageInfo.imageLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView             = textureVk->getImageView();
  imageInfo.sampler               = samplerVk->getSampler();

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet               = m_descriptorSet_;
  descriptorWrite.dstBinding           = binding;
  descriptorWrite.dstArrayElement      = 0;
  descriptorWrite.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount      = 1;
  descriptorWrite.pImageInfo           = &imageInfo;

  vkUpdateDescriptorSets(m_device_->getDevice(), 1, &descriptorWrite, 0, nullptr);
}

void DescriptorSetVk::setTexture(uint32_t binding, Texture* texture, ResourceLayout layout) {
  if (!texture) {
    GlobalLogger::Log(LogLevel::Error, "Null texture in setTexture");
    return;
  }

  TextureVk* textureVk = dynamic_cast<TextureVk*>(texture);
  if (!textureVk) {
    GlobalLogger::Log(LogLevel::Error, "Invalid texture type in setTexture");
    return;
  }

  VkDescriptorImageInfo imageInfo = {};
  imageInfo.imageLayout           = g_getImageLayoutVk(layout);
  imageInfo.imageView             = textureVk->getImageView();
  imageInfo.sampler               = VK_NULL_HANDLE;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet               = m_descriptorSet_;
  descriptorWrite.dstBinding           = binding;
  descriptorWrite.dstArrayElement      = 0;
  descriptorWrite.descriptorType       = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  descriptorWrite.descriptorCount      = 1;
  descriptorWrite.pImageInfo           = &imageInfo;

  vkUpdateDescriptorSets(m_device_->getDevice(), 1, &descriptorWrite, 0, nullptr);
}

void DescriptorSetVk::setSampler(uint32_t binding, Sampler* sampler) {
  if (!sampler) {
    GlobalLogger::Log(LogLevel::Error, "Null sampler in setSampler");
    return;
  }

  SamplerVk* samplerVk = dynamic_cast<SamplerVk*>(sampler);
  if (!samplerVk) {
    GlobalLogger::Log(LogLevel::Error, "Invalid sampler type in setSampler");
    return;
  }

  VkDescriptorImageInfo imageInfo = {};
  imageInfo.imageLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.imageView             = VK_NULL_HANDLE;
  imageInfo.sampler               = samplerVk->getSampler();

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet               = m_descriptorSet_;
  descriptorWrite.dstBinding           = binding;
  descriptorWrite.dstArrayElement      = 0;
  descriptorWrite.descriptorType       = VK_DESCRIPTOR_TYPE_SAMPLER;
  descriptorWrite.descriptorCount      = 1;
  descriptorWrite.pImageInfo           = &imageInfo;

  vkUpdateDescriptorSets(m_device_->getDevice(), 1, &descriptorWrite, 0, nullptr);
}

void DescriptorSetVk::setStorageBuffer(uint32_t binding, Buffer* buffer, uint64_t offset, uint64_t range) {
  if (!buffer) {
    GlobalLogger::Log(LogLevel::Error, "Null buffer in setStorageBuffer");
    return;
  }

  BufferVk* bufferVk = dynamic_cast<BufferVk*>(buffer);
  if (!bufferVk) {
    GlobalLogger::Log(LogLevel::Error, "Invalid buffer type in setStorageBuffer");
    return;
  }

  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer                 = bufferVk->getBuffer();
  bufferInfo.offset                 = offset;
  bufferInfo.range                  = (range == 0) ? bufferVk->getSize() - offset : range;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet               = m_descriptorSet_;
  descriptorWrite.dstBinding           = binding;
  descriptorWrite.dstArrayElement      = 0;
  descriptorWrite.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorWrite.descriptorCount      = 1;
  descriptorWrite.pBufferInfo          = &bufferInfo;

  vkUpdateDescriptorSets(m_device_->getDevice(), 1, &descriptorWrite, 0, nullptr);
}

//-------------------------------------------------------------------------
// DescriptorPoolManager implementation
//-------------------------------------------------------------------------

DescriptorPoolManager::~DescriptorPoolManager() {
  release();
}

bool DescriptorPoolManager::initialize(VkDevice device, uint32_t maxSets) {
  release();

  m_device_        = device;
  m_maxSets_       = maxSets;
  m_allocatedSets_ = 0;

  return createPool();
}

void DescriptorPoolManager::reset() {
  if (m_device_ && m_currentPool_) {
    vkResetDescriptorPool(m_device_, m_currentPool_, 0);
    m_allocatedSets_ = 0;
  }
}

void DescriptorPoolManager::release() {
  if (m_device_ && m_currentPool_) {
    vkDestroyDescriptorPool(m_device_, m_currentPool_, nullptr);
    m_currentPool_ = VK_NULL_HANDLE;
  }

  m_device_        = VK_NULL_HANDLE;
  m_maxSets_       = 0;
  m_allocatedSets_ = 0;
}

bool DescriptorPoolManager::createPool() {
  // Create a descriptor pool with common descriptor types
  // TODO: Adjust sizes based on expected usage
  std::vector<VkDescriptorPoolSize> poolSizes = {
    {               VK_DESCRIPTOR_TYPE_SAMPLER, m_maxSets_},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_maxSets_},
    {         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_maxSets_},
    {         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_maxSets_},
    {  VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, m_maxSets_},
    {  VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, m_maxSets_},
    {        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_maxSets_},
    {        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_maxSets_},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_maxSets_},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, m_maxSets_},
    {      VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, m_maxSets_}
  };

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount              = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes                 = poolSizes.data();
  poolInfo.maxSets                    = m_maxSets_;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

  if (vkCreateDescriptorPool(m_device_, &poolInfo, nullptr, &m_currentPool_) != VK_SUCCESS) {
    return false;
  }

  return true;
}

VkDescriptorSet DescriptorPoolManager::allocateDescriptorSet(VkDescriptorSetLayout layout) {
  if (!m_currentPool_) {
    if (!createPool()) {
      return VK_NULL_HANDLE;
    }
  }

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool              = m_currentPool_;
  allocInfo.descriptorSetCount          = 1;
  allocInfo.pSetLayouts                 = &layout;

  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  VkResult        result        = vkAllocateDescriptorSets(m_device_, &allocInfo, &descriptorSet);

  if (result == VK_SUCCESS) {
    m_allocatedSets_++;
    return descriptorSet;
  } else if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
    GlobalLogger::Log(LogLevel::Warning, "Descriptor pool out of memory, creating a new pool");
    reset();

    if (vkAllocateDescriptorSets(m_device_, &allocInfo, &descriptorSet) == VK_SUCCESS) {
      m_allocatedSets_++;
      return descriptorSet;
    }
  }

  GlobalLogger::Log(LogLevel::Error, "Failed to allocate descriptor set");
  return VK_NULL_HANDLE;
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise