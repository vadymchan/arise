#ifndef ARISE_RENDER_CONTEXT_H
#define ARISE_RENDER_CONTEXT_H

#include "gfx/renderer/render_settings.h"
#include "gfx/rhi/interface/command_buffer.h"
#include "gfx/rhi/interface/synchronization.h"
#include "scene/scene.h"

#include <memory>

namespace arise {
namespace gfx {
namespace renderer {

/**
 * Context holding all the information needed for rendering a frame
 */
struct RenderContext {
  Scene*                              scene = nullptr;
  std::unique_ptr<rhi::CommandBuffer> commandBuffer;
  math::Dimension2i                   viewportDimension;
  RenderSettings                      renderSettings;
  uint32_t                            currentImageIndex = 0;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_RENDER_CONTEXT_H