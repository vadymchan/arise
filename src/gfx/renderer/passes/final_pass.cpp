#include "gfx/renderer/passes/final_pass.h"

#include "gfx/renderer/frame_resources.h"
#include "profiler/profiler.h"

namespace arise {
namespace gfx {
namespace renderer {

void FinalPass::initialize(rhi::Device*           device,
                           RenderResourceManager* resourceManager,
                           FrameResources*        frameResources,
                           rhi::ShaderManager*    shaderManager) {
  m_device          = device;
  m_resourceManager = resourceManager;
  m_frameResources  = frameResources;
}

void FinalPass::render(RenderContext& context) {
  CPU_ZONE_NC("FinalPass::render", color::ORANGE);

  auto commandBuffer = context.commandBuffer.get();

  if (!commandBuffer || !m_frameResources) {
    return;
  }

  auto& renderTargets = m_frameResources->getRenderTargets(context.currentImageIndex);

  if (renderTargets.colorBuffer && renderTargets.backBuffer) {
    CPU_ZONE_NC("Copy Texture", color::YELLOW);
    GPU_ZONE_NC(commandBuffer, "Texture Copy", color::YELLOW);
    commandBuffer->copyTexture(renderTargets.colorBuffer.get(), renderTargets.backBuffer);
  }
}

void FinalPass::cleanup() {
  m_device          = nullptr;
  m_resourceManager = nullptr;
}

}  // namespace renderer
}  // namespace gfx
}  // namespace arise