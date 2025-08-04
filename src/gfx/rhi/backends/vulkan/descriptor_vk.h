#ifndef ARISE_DESCRIPTOR_VK_H
#define ARISE_DESCRIPTOR_VK_H

#include "gfx/rhi/interface/descriptor.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

class DeviceVk;
class BufferVk;
class TextureVk;
class SamplerVk;

/**
 * This class creates and manages a VkDescriptorSetLayout which defines
 * the types and bindings of resources that shaders can access.
 */
class DescriptorSetLayoutVk : public DescriptorSetLayout {
  public:
  DescriptorSetLayoutVk(const DescriptorSetLayoutDesc& desc, DeviceVk* device);
  ~DescriptorSetLayoutVk() override;

  DescriptorSetLayoutVk(const DescriptorSetLayoutVk&)            = delete;
  DescriptorSetLayoutVk& operator=(const DescriptorSetLayoutVk&) = delete;

  // Vulkan-specific methods
  VkDescriptorSetLayout getLayout() const { return m_layout_; }

  private:
  DeviceVk*             m_device_;
  VkDescriptorSetLayout m_layout_ = VK_NULL_HANDLE;
};

/**
 * This class allocates and manages a VkDescriptorSet which holds the actual
 * bindings of resources (buffers, textures, samplers) for shader access.
 */
class DescriptorSetVk : public DescriptorSet {
  public:
  DescriptorSetVk(DeviceVk* device, const DescriptorSetLayoutVk* layout);
  ~DescriptorSetVk() override;

  DescriptorSetVk(const DescriptorSetVk&)            = delete;
  DescriptorSetVk& operator=(const DescriptorSetVk&) = delete;

  // Resource binding methods
  void setUniformBuffer(uint32_t binding, Buffer* buffer, uint64_t offset = 0, uint64_t range = 0) override;
  void setTextureSampler(uint32_t binding, Texture* texture, Sampler* sampler) override;
  void setTexture(uint32_t binding, Texture* texture, ResourceLayout layout = ResourceLayout::ShaderReadOnly) override;
  void setSampler(uint32_t binding, Sampler* sampler) override;
  void setStorageBuffer(uint32_t binding, Buffer* buffer, uint64_t offset = 0, uint64_t range = 0) override;

  const DescriptorSetLayout* getLayout() const { return m_layout_; }

  // Vulkan-specific methods
  VkDescriptorSet getDescriptorSet() const { return m_descriptorSet_; }

  private:
  DeviceVk*                    m_device_;
  const DescriptorSetLayoutVk* m_layout_;
  VkDescriptorSet              m_descriptorSet_ = VK_NULL_HANDLE;
};

class DescriptorPoolManager {
  public:
  DescriptorPoolManager() = default;
  ~DescriptorPoolManager();

  bool initialize(VkDevice device, uint32_t maxSets);
  void reset();
  void release();

  VkDescriptorPool getPool() const { return m_currentPool_; }

  VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout layout);

  private:
  VkDevice         m_device_        = VK_NULL_HANDLE;
  VkDescriptorPool m_currentPool_   = VK_NULL_HANDLE;
  uint32_t         m_maxSets_       = 0;
  uint32_t         m_allocatedSets_ = 0;

  bool createPool();
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_DESCRIPTOR_VK_H