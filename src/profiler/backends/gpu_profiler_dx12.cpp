#include "profiler/backends/gpu_profiler_dx12.h"

#ifdef ARISE_USE_GPU_PROFILING

#ifdef ARISE_USE_DX12

#include "gfx/rhi/backends/dx12/command_buffer_dx12.h"
#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "utils/logger/log.h"

namespace arise {
namespace gpu {

bool GpuProfilerDx12::initialize(gfx::rhi::Device* device) {
  if (m_initialized) {
    LOG_WARN("GpuProfilerDx12 already initialized");
    return true;
  }

  if (!device) {
    LOG_ERROR("Invalid DirectX 12 parameters for profiler");
    return false;
  }

#ifdef ARISE_TRACY_GPU_PROFILING_DX12
  auto* deviceDx12 = static_cast<gfx::rhi::DeviceDx12*>(device);
  m_tracyContext   = TracyD3D12Context(deviceDx12->getDevice(), deviceDx12->getCommandQueue());

  if (!m_tracyContext) {
    LOG_ERROR("Failed to create Tracy DirectX 12 context");
    return false;
  }

  LOG_INFO("Tracy DirectX 12 profiler initialized");
#endif

  setApi(gfx::rhi::RenderingApi::Dx12);
  m_initialized = true;
  return true;
}

void GpuProfilerDx12::destroy() {
  if (!m_initialized) {
    LOG_WARN("GpuProfierDx12 already destroyed or not initialized");
    return;
  }

#ifdef ARISE_TRACY_GPU_PROFILING_DX12
  if (m_tracyContext) {
    TracyD3D12Destroy(m_tracyContext);
    m_tracyContext = nullptr;
    LOG_INFO("Tracy DirectX 12 profiler destroyed");
  }
#endif

  m_initialized = false;
}

void GpuProfilerDx12::setContextName(const std::string& name) {
  if (!m_initialized) {
    LOG_WARN("GpuProfierDx12 already destroyed or not initialized");
    return;
  }

#ifdef ARISE_TRACY_GPU_PROFILING_DX12
  if (m_tracyContext) {
    TracyD3D12ContextName(m_tracyContext, name.c_str(), name.size());
  }
#endif
}

void GpuProfilerDx12::newFrame() {
  if (!m_initialized) {
    LOG_WARN("GpuProfierDx12 already destroyed or not initialized");
    return;
  }

#ifdef ARISE_TRACY_GPU_PROFILING_DX12
  if (m_tracyContext) {
    TracyD3D12NewFrame(m_tracyContext);
  }
#endif
}

void GpuProfilerDx12::collect(gfx::rhi::CommandBuffer* commandBuffer) {
  if (!m_initialized) {
    LOG_WARN("GpuProfierDx12 already destroyed or not initialized");
    return;
  }

#ifdef ARISE_TRACY_GPU_PROFILING_DX12
  if (m_tracyContext) {
    TracyD3D12Collect(m_tracyContext);
  }
#endif
}

void GpuProfilerDx12::beginZone(gfx::rhi::CommandBuffer* cmdBuffer, const std::string& name, uint32_t color) {
  if (!cmdBuffer) {
    return;
  }

  auto colorArray = color != 0 ? color::g_toFloatArray(color).data() : nullptr;

  cmdBuffer->beginDebugMarker(name, colorArray);
}

void GpuProfilerDx12::endZone(gfx::rhi::CommandBuffer* cmdBuffer) {
  if (!cmdBuffer) {
    return;
  }
  cmdBuffer->endDebugMarker();
}

void GpuProfilerDx12::insertMarker(gfx::rhi::CommandBuffer* cmdBuffer, const std::string& name, uint32_t color) {
  if (!cmdBuffer) {
    return;
  }

  auto colorArray = color != 0 ? color::g_toFloatArray(color).data() : nullptr;

  cmdBuffer->insertDebugMarker(name, colorArray);
}

}  // namespace gpu
}  // namespace arise

#endif  // ARISE_USE_DX12

#endif  // ARISE_USE_GPU_PROFILING