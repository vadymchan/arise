#ifndef ARISE_PROFILER_CONFIG_H
#define ARISE_PROFILER_CONFIG_H

#ifdef ARISE_USE_TRACY_GPU_PROFILING
#ifdef ARISE_FORCE_VULKAN
#define ARISE_TRACY_GPU_PROFILING_VK
#elif defined(ARISE_FORCE_DIRECTX)
#define ARISE_TRACY_GPU_PROFILING_DX12
#else
#error \
    "Tracy GPU profiling requires FORCE_RHI_API to be enabled. Please set FORCE_RHI_API=ON in cmake and select specific API."
#endif
#endif

#endif  // ARISE_PROFILER_CONFIG_H