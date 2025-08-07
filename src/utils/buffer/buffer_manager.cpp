#include "utils/buffer/buffer_manager.h"

#include "gfx/rhi/interface/device.h"
#include "utils/logger/log.h"
#include "utils/memory/align.h"
#include "utils/resource/resource_deletion_manager.h"
#include "utils/service/service_locator.h"

namespace arise {

BufferManager::BufferManager(gfx::rhi::Device* device)
    : m_device(device)
    , m_bufferCounter(0) {
  if (!m_device) {
    LOG_ERROR("Device is null");
  }
}

BufferManager::~BufferManager() {
  if (!m_buffers.empty()) {
    LOG_INFO("BufferManager destroyed, releasing {} buffers", m_buffers.size());

    for (const auto& [name, buffer] : m_buffers) {
      LOG_INFO("Released buffer: {}", name);
    }
  }
  release();
}

gfx::rhi::Buffer* BufferManager::createVertexBuffer(const void*        data,
                                                    size_t             vertexCount,
                                                    size_t             vertexStride,
                                                    const std::string& name) {
  if (!m_device) {
    LOG_ERROR("Cannot create vertex buffer, device is null");
    return nullptr;
  }

  if (!data || vertexCount == 0 || vertexStride == 0) {
    LOG_ERROR("Invalid vertex buffer parameters");
    return nullptr;
  }

  const size_t bufferSize = vertexCount * vertexStride;

  gfx::rhi::BufferDesc bufferDesc;
  bufferDesc.size        = bufferSize;
  bufferDesc.type        = gfx::rhi::BufferType::Static;
  bufferDesc.createFlags = gfx::rhi::BufferCreateFlag::VertexBuffer;
  bufferDesc.stride      = vertexStride;
  bufferDesc.debugName   = name.empty() ? "unnamed_vertex_buffer" : name;

  std::string bufferName = name.empty() ? generateUniqueName_("VertexBuffer") : name;

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_buffers.contains(bufferName)) {
    LOG_WARN("Buffer with name '{}' already exists, will be replaced", bufferName);
    m_buffers.erase(bufferName);
  }

  auto buffer = m_device->createBuffer(bufferDesc);
  if (!buffer) {
    LOG_ERROR("Failed to create vertex buffer '{}'", bufferName);
    return nullptr;
  }

  m_device->updateBuffer(buffer.get(), data, bufferSize);

  gfx::rhi::Buffer* bufferPtr = buffer.get();
  m_buffers[bufferName]       = std::move(buffer);

  LOG_INFO("Created vertex buffer '{}' with {} vertices", bufferName, vertexCount);

  return bufferPtr;
}

gfx::rhi::Buffer* BufferManager::createIndexBuffer(const void*        data,
                                                   size_t             indexCount,
                                                   size_t             indexSize,
                                                   const std::string& name) {
  if (!m_device) {
    LOG_ERROR("Cannot create index buffer, device is null");
    return nullptr;
  }

  if (!data || indexCount == 0 || (indexSize != 2 && indexSize != 4)) {
    LOG_ERROR("Invalid index buffer parameters");
    return nullptr;
  }

  const size_t bufferSize = indexCount * indexSize;

  gfx::rhi::BufferDesc bufferDesc;
  bufferDesc.size        = bufferSize;
  bufferDesc.type        = gfx::rhi::BufferType::Static;
  bufferDesc.createFlags = gfx::rhi::BufferCreateFlag::IndexBuffer;
  bufferDesc.debugName   = name.empty() ? "unnamed_index_buffer" : name;

  std::string bufferName = name.empty() ? generateUniqueName_("IndexBuffer") : name;

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_buffers.contains(bufferName)) {
    LOG_WARN("Buffer with name '{}' already exists, will be replaced", bufferName);
    m_buffers.erase(bufferName);
  }

  auto buffer = m_device->createBuffer(bufferDesc);
  if (!buffer) {
    LOG_ERROR("Failed to create index buffer '{}'", bufferName);
    return nullptr;
  }

  m_device->updateBuffer(buffer.get(), data, bufferSize);

  gfx::rhi::Buffer* bufferPtr = buffer.get();
  m_buffers[bufferName]       = std::move(buffer);

  LOG_INFO("Created index buffer '{}' with {} indices", bufferName, indexCount);

  return bufferPtr;
}

gfx::rhi::Buffer* BufferManager::createUniformBuffer(size_t size, const void* data, const std::string& name) {
  if (!m_device) {
    LOG_ERROR("Cannot create uniform buffer, device is null");
    return nullptr;
  }

  if (size == 0) {
    LOG_ERROR("Invalid uniform buffer size");
    return nullptr;
  }

  gfx::rhi::BufferDesc bufferDesc;
  bufferDesc.size        = alignConstantBufferSize(size);
  bufferDesc.type        = gfx::rhi::BufferType::Dynamic;  // Uniform buffers are typically updated frequently
  bufferDesc.createFlags = gfx::rhi::BufferCreateFlag::CpuAccess | gfx::rhi::BufferCreateFlag::ConstantBuffer;
  bufferDesc.debugName   = name.empty() ? "unnamed_uniform_buffer" : name;

  std::string bufferName = name.empty() ? generateUniqueName_("UniformBuffer") : name;

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_buffers.contains(bufferName)) {
    LOG_WARN("Buffer with name '{}' already exists, will be replaced", bufferName);
    m_buffers.erase(bufferName);
  }

  auto buffer = m_device->createBuffer(bufferDesc);
  if (!buffer) {
    LOG_ERROR("Failed to create uniform buffer '{}'", bufferName);
    return nullptr;
  }

  if (data) {
    m_device->updateBuffer(buffer.get(), data, size);
  }

  gfx::rhi::Buffer* bufferPtr = buffer.get();
  m_buffers[bufferName]       = std::move(buffer);

  LOG_INFO("Created uniform buffer '{}' with size {} bytes", bufferName, size);

  return bufferPtr;
}

gfx::rhi::Buffer* BufferManager::createStorageBuffer(size_t size, const void* data, const std::string& name) {
  if (!m_device) {
    LOG_ERROR("Cannot create storage buffer, device is null");
    return nullptr;
  }

  if (size == 0) {
    LOG_ERROR("Invalid storage buffer size");
    return nullptr;
  }

  gfx::rhi::BufferDesc bufferDesc;
  bufferDesc.size = size;
  bufferDesc.type = gfx::rhi::BufferType::Dynamic;

  bufferDesc.createFlags
      = static_cast<gfx::rhi::BufferCreateFlag>(static_cast<uint32_t>(gfx::rhi::BufferCreateFlag::CpuAccess)
                                                | static_cast<uint32_t>(gfx::rhi::BufferCreateFlag::Uav));
  bufferDesc.debugName = name.empty() ? "unnamed_storage_buffer" : name;

  std::string bufferName = name.empty() ? generateUniqueName_("StorageBuffer") : name;

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_buffers.contains(bufferName)) {
    LOG_WARN("Buffer with name '{}' already exists, will be replaced", bufferName);
    m_buffers.erase(bufferName);
  }

  auto buffer = m_device->createBuffer(bufferDesc);
  if (!buffer) {
    LOG_ERROR("Failed to create storage buffer '{}'", bufferName);
    return nullptr;
  }

  if (data) {
    m_device->updateBuffer(buffer.get(), data, size);
  }

  gfx::rhi::Buffer* bufferPtr = buffer.get();
  m_buffers[bufferName]       = std::move(buffer);

  LOG_INFO("Created storage buffer '{}' with size {} bytes", bufferName, size);

  return bufferPtr;
}

gfx::rhi::Buffer* BufferManager::addBuffer(std::unique_ptr<gfx::rhi::Buffer> buffer, const std::string& name) {
  if (!buffer) {
    LOG_ERROR("Cannot add null buffer");
    return nullptr;
  }

  std::string bufferName = name.empty() ? generateUniqueName_("Buffer") : name;

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_buffers.contains(bufferName)) {
    LOG_WARN("Buffer with name '{}' already exists, will be replaced", bufferName);
    m_buffers.erase(bufferName);
  }

  gfx::rhi::Buffer* bufferPtr = buffer.get();
  m_buffers[bufferName]       = std::move(buffer);

  LOG_INFO("Added external buffer '{}'", bufferName);

  return bufferPtr;
}

gfx::rhi::Buffer* BufferManager::getBuffer(const std::string& name) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_buffers.find(name);
  if (it != m_buffers.end()) {
    return it->second.get();
  }

  return nullptr;
}

bool BufferManager::removeBuffer(const std::string& name) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_buffers.find(name);
  if (it != m_buffers.end()) {
    LOG_INFO("Removing buffer '{}'", name);
    m_buffers.erase(it);
    return true;
  }

  LOG_WARN("Attempted to remove non-existent buffer '{}'", name);
  return false;
}

bool BufferManager::removeBuffer(gfx::rhi::Buffer* buffer) {
  if (!buffer) {
    LOG_ERROR("Cannot remove null buffer");
    return false;
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
    if (it->second.get() == buffer) {
      auto deletionManager = ServiceLocator::s_get<ResourceDeletionManager>();
      if (deletionManager) {
        std::string bufferName = it->first;

        deletionManager->enqueueForDeletion<gfx::rhi::Buffer>(
            buffer,
            [this, bufferName](gfx::rhi::Buffer*) {
              std::lock_guard<std::mutex> lock(m_mutex);
              LOG_INFO("Buffer '{}' deleted", bufferName);
              m_buffers.erase(bufferName);
            },
            bufferName,
            "Buffer");

        return true;
      } else {
        LOG_INFO("Removing buffer: {}", it->first);
        m_buffers.erase(it);
        return true;
      }
    }
  }

  LOG_WARN("Buffer not found in manager");
  return false;
}

bool BufferManager::updateBuffer(const std::string& name, const void* data, size_t size, size_t offset) {
  if (!m_device || !data || size == 0) {
    LOG_ERROR("Invalid update buffer parameters");
    return false;
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_buffers.find(name);
  if (it == m_buffers.end()) {
    LOG_ERROR("Cannot update buffer '{}', not found", name);
    return false;
  }

  m_device->updateBuffer(it->second.get(), data, size, offset);
  return true;
}

bool BufferManager::updateBuffer(gfx::rhi::Buffer* buffer, const void* data, size_t size, size_t offset) {
  if (!m_device || !buffer || !data || size == 0) {
    LOG_ERROR("Invalid update buffer parameters");
    return false;
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = findBuffer_(buffer);
  if (it == m_buffers.end()) {
    LOG_WARN("Updating buffer not managed by this BufferManager");
  }

  m_device->updateBuffer(buffer, data, size, offset);
  return true;
}

void BufferManager::release() {
  std::lock_guard<std::mutex> lock(m_mutex);

  LOG_INFO("Releasing {} buffers", m_buffers.size());

  m_buffers.clear();
}

std::string BufferManager::generateUniqueName_(const std::string& prefix) {
  return prefix + "_" + std::to_string(m_bufferCounter++);
}

std::unordered_map<std::string, std::unique_ptr<gfx::rhi::Buffer>>::iterator BufferManager::findBuffer_(
    const std::string& name) {
  return m_buffers.find(name);
}

std::unordered_map<std::string, std::unique_ptr<gfx::rhi::Buffer>>::iterator BufferManager::findBuffer_(
    const gfx::rhi::Buffer* buffer) {
  for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
    if (it->second.get() == buffer) {
      return it;
    }
  }
  return m_buffers.end();
}

}  // namespace arise