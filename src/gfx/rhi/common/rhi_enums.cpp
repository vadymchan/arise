#include "gfx/rhi/common/rhi_enums.h"

#include "utils/logger/log.h"

#ifdef ARISE_USE_VULKAN
#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"
#endif

#ifdef ARISE_USE_DX12
#include "gfx/rhi/backends/dx12/rhi_enums_dx12.h"
#endif

#include <cassert>

namespace arise {
namespace gfx {
namespace rhi {

int g_getTextureComponentCount(TextureFormat format, RenderingApi api) {
  switch (api) {
#ifdef ARISE_USE_VULKAN
    case RenderingApi::Vulkan:
      return g_getTextureComponentCountVk(format);
#endif

#ifdef ARISE_USE_DX12
    case RenderingApi::Dx12:
      return g_getTextureComponentCountDx12(format);
#endif

    default:
      // TODO: add logger with assertion (prob macro function)
      LOG_ERROR("Unsupported rendering API for g_getTextureComponentCount: {}", static_cast<int>(api));
      assert(false && "Unsupported rendering API");
      return 0;
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise