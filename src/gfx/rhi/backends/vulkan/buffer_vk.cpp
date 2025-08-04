#include "gfx/rhi/backends/vulkan/buffer_vk.h"

#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"
#include "utils/logger/global_logger.h"

namespace arise {
namespace gfx {
namespace rhi {

BufferVk::BufferVk(const BufferDesc& desc, DeviceVk* device)
    : Buffer(desc)
    , m_device_(device) {
  VkBufferUsageFlags    usage    = getBufferUsageFlags_();
  VkMemoryPropertyFlags memProps = getMemoryPropertyFlags_();

  if (!createBuffer_(usage, memProps)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create Vulkan buffer");
    return;
  }

  // If this is a CPU-accessible and dynamic buffer, the memory is already mapped by VMA
  if ((memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      && (m_desc_.type == BufferType::Dynamic
          || ((m_desc_.createFlags & BufferCreateFlag::CpuAccess) != BufferCreateFlag::None))) {
    if (m_allocationInfo_.pMappedData != nullptr) {
      m_mappedData_ = m_allocationInfo_.pMappedData;
      m_isMapped_   = true;
    }
  }

  if (!desc.debugName.empty() && m_buffer_ != VK_NULL_HANDLE) {
    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType   = VK_OBJECT_TYPE_BUFFER;
    nameInfo.objectHandle = (uint64_t)m_buffer_;
    nameInfo.pObjectName  = desc.debugName.c_str();

    auto fpSetDebugUtilsObjectName
        = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(m_device_->getDevice(), "vkSetDebugUtilsObjectNameEXT");

    if (fpSetDebugUtilsObjectName) {
      fpSetDebugUtilsObjectName(m_device_->getDevice(), &nameInfo);
    }
  }
}

BufferVk::~BufferVk() {
  if (m_device_ && m_buffer_ != VK_NULL_HANDLE) {
    vmaDestroyBuffer(m_device_->getAllocator(), m_buffer_, m_allocation_);
    m_buffer_     = VK_NULL_HANDLE;
    m_allocation_ = VK_NULL_HANDLE;
    m_mappedData_ = nullptr;
    m_isMapped_   = false;
  }
}

bool BufferVk::update_(const void* data, size_t size, size_t offset) {
  if (!data || size == 0) {
    return false;
  }

  if (offset + size > m_desc_.size) {
    GlobalLogger::Log(LogLevel::Error, "Buffer update exceeds buffer size");
    return false;
  }

  if (m_isMapped_ && m_mappedData_) {
    char* mappedCharPtr = static_cast<char*>(m_mappedData_);
    memcpy(mappedCharPtr + offset, data, size);

    if (!(getMemoryPropertyFlags_() & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
      VmaAllocationInfo allocInfo;
      vmaGetAllocationInfo(m_device_->getAllocator(), m_allocation_, &allocInfo);

      VkMappedMemoryRange memRange = {};
      memRange.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
      memRange.memory              = allocInfo.deviceMemory;
      memRange.offset              = allocInfo.offset + offset;
      memRange.size                = size;
      vkFlushMappedMemoryRanges(m_device_->getDevice(), 1, &memRange);
    }
    return true;
  }

  return false;
}

bool BufferVk::map_(void** ppData) {
  if (!ppData) {
    return false;
  }

  if (m_isMapped_) {
    *ppData = m_mappedData_;
    return true;
  }

  VkMemoryPropertyFlags memProps = getMemoryPropertyFlags_();
  if (!(memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
    GlobalLogger::Log(LogLevel::Error, "Cannot map a non-host-visible buffer");
    return false;
  }

  VkResult result = vmaMapMemory(m_device_->getAllocator(), m_allocation_, ppData);
  if (result != VK_SUCCESS) {
    GlobalLogger::Log(LogLevel::Error, "Failed to map buffer memory");
    return false;
  }

  m_mappedData_ = *ppData;
  m_isMapped_   = true;
  return true;
}

void BufferVk::unmap_() {
  if (!m_isMapped_) {
    return;
  }

  vmaUnmapMemory(m_device_->getAllocator(), m_allocation_);
  m_mappedData_ = nullptr;
  m_isMapped_   = false;
}

bool BufferVk::createBuffer_(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size               = m_desc_.size;
  bufferInfo.usage              = usage;
  bufferInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};

  if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    if (properties & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
      // For CPU reads (readback)
      allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    } else {
      // For CPU writes to GPU
      allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    }

    if (m_desc_.type == BufferType::Dynamic
        || (m_desc_.createFlags & BufferCreateFlag::CpuAccess) != BufferCreateFlag::None) {
      allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
  } else {
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  }

  allocInfo.requiredFlags = properties;

  VkResult result = vmaCreateBuffer(
      m_device_->getAllocator(), &bufferInfo, &allocInfo, &m_buffer_, &m_allocation_, &m_allocationInfo_);

  return result == VK_SUCCESS;
}

VkBufferUsageFlags BufferVk::getBufferUsageFlags_() const {
  VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  if ((m_desc_.createFlags & BufferCreateFlag::Readback) != BufferCreateFlag::None) {
    usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::VertexBuffer) != BufferCreateFlag::None) {
    usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::InstanceBuffer) != BufferCreateFlag::None) {
    usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::IndexBuffer) != BufferCreateFlag::None) {
    usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::Uav) != BufferCreateFlag::None) {
    usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::ShaderResource) != BufferCreateFlag::None) {
    if (m_desc_.stride > 0) {
      usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    } else {
      usage |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    }
  }

  if ((m_desc_.createFlags & BufferCreateFlag::IndirectCommand) != BufferCreateFlag::None) {
    usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::AccelerationStructure) != BufferCreateFlag::None) {
    usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::AccelerationStructureBuildInput) != BufferCreateFlag::None) {
    usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::ShaderBindingTable) != BufferCreateFlag::None) {
    usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
  }

  // If no specific flags are set, assume it's a uniform buffer
  if (!(usage
        & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
           | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT))) {
    usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }

  return usage;
}

VkMemoryPropertyFlags BufferVk::getMemoryPropertyFlags_() const {
  VkMemoryPropertyFlags props = 0;

  if (m_desc_.type == BufferType::Static) {
    props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if ((m_desc_.createFlags & BufferCreateFlag::CpuAccess) != BufferCreateFlag::None) {
      props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
  } else if (m_desc_.type == BufferType::Dynamic) {
    props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  }

  if ((m_desc_.createFlags & BufferCreateFlag::Readback) != BufferCreateFlag::None) {
    props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  }

  return props;
}

uint32_t BufferVk::findMemoryType_(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(m_device_->getPhysicalDevice(), &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  GlobalLogger::Log(LogLevel::Error, "Failed to find suitable memory type for buffer");
  return 0;
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise
