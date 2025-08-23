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
 * Statistics collected during frame rendering
 */
struct RenderStatistics {
  uint32_t drawCalls         = 0;
  uint32_t trianglesRendered = 0;
  uint32_t instancesRendered = 0;
  uint32_t verticesProcessed = 0;
  uint32_t setPassCalls      = 0;  // Pipeline switches
  uint32_t batches           = 0;  // Number of draw call batches

  void reset() {
    drawCalls         = 0;
    trianglesRendered = 0;
    instancesRendered = 0;
    verticesProcessed = 0;
    setPassCalls      = 0;
    batches           = 0;
  }
};

/**
 * Context holding all the information needed for rendering a frame
 */
struct RenderContext {
  Scene*                              scene = nullptr;
  std::unique_ptr<rhi::CommandBuffer> commandBuffer;
  math::Dimension2i                   viewportDimension;
  RenderSettings                      renderSettings;
  uint32_t                            currentImageIndex = 0;
  RenderStatistics                    statistics;
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_RENDER_CONTEXT_H