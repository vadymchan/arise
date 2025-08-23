#ifndef ARISE_RENDERER_RENDER_PASS_H
#define ARISE_RENDERER_RENDER_PASS_H

#include "gfx/renderer/render_context.h"

#include <math_library/dimension.h>

namespace arise::gfx::rhi {
class Device;
class ShaderManager;
}  // namespace arise::gfx::rhi

namespace arise {
namespace gfx {
namespace renderer {

class RenderResourceManager;
class FrameResources;

/**
 * Defines the interface for render passes and their lifecycle
 */
class RenderPass {
  public:
  RenderPass()          = default;
  virtual ~RenderPass() = default;

  RenderPass(const RenderPass&)            = delete;
  RenderPass& operator=(const RenderPass&) = delete;
  RenderPass(RenderPass&&)                 = delete;
  RenderPass& operator=(RenderPass&&)      = delete;

  /**
   * Called ONCE during creation
   * Creates resources that don't depend on screen size:
   * - Descriptor set layouts
   * - Shaders
   * - Base pipeline settings
   * - RenderPass objects
   */
  virtual void initialize(rhi::Device*           device,
                          RenderResourceManager* resourceManager,
                          FrameResources*        frameResources,
                          rhi::ShaderManager*    shaderManager)
      = 0;

  /**
   * Called when window/viewport size changes
   * Recreates resources that depend on dimensions:
   * - Framebuffers
   * - Updates viewport/scissor
   */
  virtual void resize(const math::Dimension2i& newDimension) = 0;

  /**
   * Called at the beginning of each frame BEFORE render()
   * Prepares resources for the current frame:
   * - Filters and sorts objects for rendering
   * - Updates pass-specific UBOs
   * - Prepares pass-specific descriptor sets
   */
  virtual void prepareFrame(const RenderContext& context) = 0;

  /**
   * Called to record rendering commands
   * Performs the main rendering work:
   * - Resource state transitions
   * - beginRenderPass with correct buffers
   * - Sets viewport/scissor
   * - Binds descriptors and records draw commands
   * - endRenderPass
   */
  virtual void render(RenderContext& context) = 0;

  /**
   * Called at the end of each frame (optional)
   * Cleans up temporary resources:
   * - Resets descriptor pools
   * - Clears temporary buffers
   */
  virtual void endFrame() {}

  /**
   * Called during destruction to free resources:
   * - All resources created in initialize()
   * - Framebuffer, renderPass, pipelines
   * - Descriptor sets
   */
  virtual void cleanup() = 0;

  virtual bool isExclusive() const { return false; }
};

}  // namespace renderer
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_RENDERER_RENDER_PASS_H