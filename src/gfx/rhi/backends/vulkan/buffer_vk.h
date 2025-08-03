#ifndef ARISE_BUFFER_VK_H
#define ARISE_BUFFER_VK_H

#include "gfx/rhi/interface/buffer.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace arise {
namespace gfx {
namespace rhi {

class DeviceVk;

class BufferVk : public Buffer {
  public:
  BufferVk(const BufferDesc& desc, DeviceVk* device);
  ~BufferVk() override;

  VkBuffer getBuffer() const { return m_buffer_; }

  VmaAllocation getAllocation() const { return m_allocation_; }

  uint32_t getStride() const { return m_stride_; }

  private:
  friend class DeviceVk;
  // update_ method is called from DeviceVk
  bool update_(const void* data, size_t size, size_t offset);

  bool map_(void** ppData);
  void unmap_();

  bool createBuffer_(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

  VkBufferUsageFlags getBufferUsageFlags_() const;

  VkMemoryPropertyFlags getMemoryPropertyFlags_() const;

  uint32_t findMemoryType_(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

  DeviceVk*         m_device_     = nullptr;
  VkBuffer          m_buffer_     = VK_NULL_HANDLE;
  VmaAllocation     m_allocation_ = VK_NULL_HANDLE;
  VmaAllocationInfo m_allocationInfo_{};
  uint32_t          m_stride_     = 0;        // For vertex buffers
  void*             m_mappedData_ = nullptr;  // For persistent mapping
  bool              m_isMapped_   = false;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_BUFFER_VK_H