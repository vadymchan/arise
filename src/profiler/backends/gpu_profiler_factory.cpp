#include "profiler/backends/gpu_profiler_factory.h"

#ifdef ARISE_USE_GPU_PROFILING

#include "profiler/backends/gpu_profiler_dx12.h"
#include "profiler/backends/gpu_profiler_vk.h"
#include "utils/logger/log.h"

namespace arise {
namespace gpu {

std::unique_ptr<GpuProfiler> GpuProfilerFactory::create(gfx::rhi::RenderingApi api) {
#ifndef ARISE_USE_GPU_PROFILING
  return nullptr;
#endif

  std::unique_ptr<GpuProfiler> profiler;
  switch (api) {
#ifdef ARISE_USE_VULKAN
    case gfx::rhi::RenderingApi::Vulkan:
      profiler = std::make_unique<GpuProfilerVk>();
      break;
#endif

#ifdef ARISE_USE_DX12
    case gfx::rhi::RenderingApi::Dx12:
      profiler = std::make_unique<GpuProfilerDx12>();
      break;
#endif

    default:
      LOG_ERROR("Unsupported rendering API for GPU profiler");
      return nullptr;
  }
  profiler->setApi(api);
  return profiler;
}

}  // namespace gpu
}  // namespace arise

#endif  // ARISE_USE_GPU_PROFILING
