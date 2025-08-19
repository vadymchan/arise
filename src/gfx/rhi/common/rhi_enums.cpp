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

int g_getVertexFormatComponentCount(VertexFormat format) {
  switch (format) {
    case VertexFormat::R8:
    case VertexFormat::R8ui:
    case VertexFormat::R16f:
    case VertexFormat::R32f:
    case VertexFormat::R32ui:
    case VertexFormat::R32si:
      return 1;
    case VertexFormat::Rg8:
    case VertexFormat::Rg16f:
    case VertexFormat::Rg32f:
      return 2;
    case VertexFormat::Rgb8:
    case VertexFormat::Rgb16f:
    case VertexFormat::Rgb32f:
      return 3;
    case VertexFormat::Rgba8:
    case VertexFormat::Rgba16f:
    case VertexFormat::Rgba32f:
    case VertexFormat::Rgba8ui:
    case VertexFormat::Rgba8si:
      return 4;
    case VertexFormat::Count:
    default:
      LOG_ERROR("Unknown VertexFormat: {}", static_cast<int>(format));
      assert(false && "Unknown VertexFormat");
      return 0;
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise