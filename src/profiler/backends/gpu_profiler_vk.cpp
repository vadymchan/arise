#include "profiler/backends/gpu_profiler_vk.h"

#ifdef ARISE_USE_GPU_PROFILING

#ifdef ARISE_USE_VULKAN

#include "gfx/rhi/backends/vulkan/command_buffer_vk.h"
#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "utils/logger/log.h"

namespace arise {
namespace gpu {

bool GpuProfilerVk::initialize(gfx::rhi::Device* device) {
  if (m_initialized) {
    LOG_WARN("GpuProfilerVk already initialized");
    return true;
  }

  if (!device) {
    LOG_ERROR("Invalid Vulkan parameters for profiler");
    return false;
  }

#ifdef ARISE_TRACY_GPU_PROFILING_VK
  m_cmdBuffer      = device->createCommandBuffer();
  auto cmdBufferVk = static_cast<gfx::rhi::CommandBufferVk*>(m_cmdBuffer.get());

  auto* deviceVk = static_cast<gfx::rhi::DeviceVk*>(device);

  m_tracyContext = TracyVkContext(deviceVk->getPhysicalDevice(),
                                  deviceVk->getDevice(),
                                  deviceVk->getGraphicsQueue(),
                                  cmdBufferVk->getCommandBuffer());

  if (!m_tracyContext) {
    LOG_ERROR("Failed to create Tracy Vulkan context");
    return false;
  }

  LOG_INFO("Tracy Vulkan profiler initialized");
#else
  LOG_INFO("Tracy profiling disabled, using native GPU markers only");
#endif

  setApi(gfx::rhi::RenderingApi::Vulkan);
  m_initialized = true;
  return true;
}

void GpuProfilerVk::destroy() {
  if (!m_initialized) {
    return;
  }

#ifdef ARISE_TRACY_GPU_PROFILING_VK
  if (m_tracyContext) {
    TracyVkDestroy(m_tracyContext);
    m_tracyContext = nullptr;
    LOG_INFO("Tracy Vulkan profiler destroyed");
  }
#endif

  m_initialized = false;
}

void GpuProfilerVk::setContextName(const std::string& name) {
  if (!m_initialized) {
    return;
  }

#ifdef ARISE_TRACY_GPU_PROFILING_VK
  if (m_tracyContext) {
    TracyVkContextName(m_tracyContext, name.c_str(), name.size());
  }
#endif
}

void GpuProfilerVk::collect(gfx::rhi::CommandBuffer* commandBuffer) {
  if (!m_initialized) {
    return;
  }

#ifdef ARISE_TRACY_GPU_PROFILING_VK
  if (m_tracyContext && commandBuffer) {
    auto commandBufferVk = static_cast<gfx::rhi::CommandBufferVk*>(commandBuffer);
    TracyVkCollect(m_tracyContext, commandBufferVk->getCommandBuffer());
  }
#endif
}

void GpuProfilerVk::beginZone(gfx::rhi::CommandBuffer* cmdBuffer, const std::string& name, uint32_t color) {
  if (!cmdBuffer) {
    return;
  }

  auto colorArray = color != 0 ? color::g_toFloatArray(color).data() : nullptr;

  cmdBuffer->beginDebugMarker(name, colorArray);
}

void GpuProfilerVk::endZone(gfx::rhi::CommandBuffer* cmdBuffer) {
  if (!cmdBuffer) {
    return;
  }
  cmdBuffer->endDebugMarker();
}

void GpuProfilerVk::insertMarker(gfx::rhi::CommandBuffer* cmdBuffer, const std::string& name, uint32_t color) {
  if (!cmdBuffer) {
    return;
  }

  auto colorArray = color != 0 ? color::g_toFloatArray(color).data() : nullptr;

  cmdBuffer->insertDebugMarker(name, colorArray);
}

}  // namespace gpu
}  // namespace arise

#endif  // ARISE_USE_GPU_PROFILING

#endif  // ARISE_USE_VULKAN