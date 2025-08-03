#include "gfx/renderer/renderer.h"

#include "ecs/components/camera.h"
#include "ecs/systems/light_system.h"
#include "ecs/systems/system_manager.h"
#include "gfx/rhi/backends/dx12/command_buffer_dx12.h"
#include "gfx/rhi/backends/dx12/device_dx12.h"
#include "gfx/rhi/backends/vulkan/command_buffer_vk.h"
#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "gfx/rhi/common/rhi_creators.h"
#include "platform/common/window.h"
#include "profiler/profiler.h"
#include "scene/scene_manager.h"
#include "utils/buffer/buffer_manager.h"
#include "utils/frame_manager/frame_manager.h"
#include "utils/material/material_manager.h"
#include "utils/model/render_geometry_mesh_manager.h"
#include "utils/model/render_mesh_manager.h"
#include "utils/model/render_model_manager.h"
#include "utils/resource/resource_deletion_manager.h"
#include "utils/texture/texture_manager.h"

namespace arise {
namespace gfx {
namespace renderer {
bool Renderer::initialize(Window* window, rhi::RenderingApi api) {
  m_window = window;

  rhi::DeviceDesc deviceDesc;
  deviceDesc.window = window;
  m_device          = g_createDevice(api, deviceDesc);

  if (!m_device) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create device");
    return false;
  }

  auto     frameManager   = ServiceLocator::s_get<FrameManager>();
  uint32_t framesInFlight = frameManager->getMaxFramesInFlight();

  m_commandBufferPools.resize(framesInFlight);
  m_frameFences.resize(framesInFlight);
  m_imageAvailableSemaphores.resize(framesInFlight);
  m_renderFinishedSemaphores.resize(framesInFlight);

  // TODO: move to a separate function
  rhi::SwapchainDesc swapchainDesc;
  swapchainDesc.width       = window->getSize().width();
  swapchainDesc.height      = window->getSize().height();
  swapchainDesc.format      = rhi::TextureFormat::Bgra8;
  swapchainDesc.bufferCount = framesInFlight;

  m_swapChain = m_device->createSwapChain(swapchainDesc);
  if (!m_swapChain) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create swap chain");
    return false;
  }

  m_shaderManager   = std::make_unique<rhi::ShaderManager>(m_device.get(), framesInFlight, true);
  m_resourceManager = std::make_unique<RenderResourceManager>();

  m_frameResources = std::make_unique<FrameResources>(m_device.get(), getResourceManager());
  m_frameResources->initialize(framesInFlight);
  m_frameResources->resize(window->getSize());

  // synchronization (move to a separate function)
  for (uint32_t i = 0; i < framesInFlight; ++i) {
    rhi::FenceDesc fenceDesc;
    fenceDesc.signaled = true;
    m_frameFences[i]   = m_device->createFence(fenceDesc);

    m_imageAvailableSemaphores[i] = m_device->createSemaphore();
    m_renderFinishedSemaphores[i] = m_device->createSemaphore();

    m_commandBufferPools[i].clear();
    m_commandBufferPools[i].reserve(COMMAND_BUFFERS_PER_FRAME);

    for (uint32_t j = 0; j < INITIAL_COMMAND_BUFFERS; ++j) {
      rhi::CommandBufferDesc cmdDesc;
      auto                   cmdBuffer = m_device->createCommandBuffer(cmdDesc);
      if (cmdBuffer) {
        m_commandBufferPools[i].emplace_back(std::move(cmdBuffer));
      }
    }
  }

  setupRenderPasses_();

  auto deletionManager = ServiceLocator::s_get<ResourceDeletionManager>();
  if (deletionManager) {
    deletionManager->setDefaultFrameDelay(framesInFlight);
  }

  initializeGpuProfiler_();

  m_initialized = true;

  GlobalLogger::Log(LogLevel::Info, "Renderer initialized successfully");
  return true;
}

RenderContext Renderer::beginFrame(Scene* scene, const RenderSettings& renderSettings) {
  CPU_ZONE_NC("Renderer::beginFrame", color::PURPLE);
  if (!m_initialized) {
    GlobalLogger::Log(LogLevel::Error, "Renderer not initialized");
    return RenderContext();
  }

  auto frameManager = ServiceLocator::s_get<FrameManager>();

  {
    CPU_ZONE_NC("Frame Synchronization", color::PURPLE);
    auto  currentFrameIndex = frameManager->getCurrentFrameIndex();
    auto& fence             = m_frameFences[currentFrameIndex];

    fence->wait();
    fence->reset();
  }

  {
    CPU_ZONE_NC("Profiler Setup", color::PURPLE);
    if (auto* profiler = ServiceLocator::s_get<gpu::GpuProfiler>()) {
      profiler->newFrame();
    }

    auto deletionManager = ServiceLocator::s_get<ResourceDeletionManager>();
    if (deletionManager) {
      auto frameIndex = frameManager->getTotalFrameCount();
      deletionManager->setCurrentFrame(frameIndex);
    }
  }

  {
    CPU_ZONE_NC("Resource Updates", color::PURPLE);
    m_resourceManager->updateScheduledPipelines();
  }

  {
    CPU_ZONE_NC("Swapchain Acquisition", color::PURPLE);
    auto currentFrameIndex = frameManager->getCurrentFrameIndex();
    if (!m_swapChain->acquireNextImage(m_imageAvailableSemaphores[currentFrameIndex].get())) {
      GlobalLogger::Log(LogLevel::Error, "Failed to acquire next swapchain image");
      return RenderContext();
    }
  }

  auto commandBuffer = acquireCommandBuffer_();
  commandBuffer->begin();

  GPU_ZONE_NC(commandBuffer.get(), "Begin Frame", color::PURPLE);

  {
    GPU_ZONE_NC(commandBuffer.get(), "Descriptor Heap Setup", color::PURPLE);
#ifdef ARISE_USE_DX12
    if (m_device->getApiType() == rhi::RenderingApi::Dx12) {
      auto dx12CmdBuffer = static_cast<rhi::CommandBufferDx12*>(commandBuffer.get());
      dx12CmdBuffer->bindDescriptorHeaps();
    }
#endif  //  ARISE_USE_DX12
  }

  {
    CPU_ZONE_NC("Viewport Setup", color::PURPLE);
    math::Dimension2i viewportDimension;
    switch (renderSettings.appMode) {
      case ApplicationMode::Standalone:
        viewportDimension = m_window->getSize();
        break;
      case ApplicationMode::Editor:
        viewportDimension = renderSettings.renderViewportDimension;
        onViewportResize(viewportDimension);
        break;
      default:
        GlobalLogger::Log(LogLevel::Error, "Invalid application mode");
        return RenderContext();
    }

    auto  swapchainImage    = m_swapChain->getCurrentImage();
    auto& renderTarget      = m_frameResources->getRenderTargets(m_swapChain->getCurrentImageIndex());
    renderTarget.backBuffer = swapchainImage;

    RenderContext context;
    context.scene             = scene;
    context.commandBuffer     = std::move(commandBuffer);
    context.viewportDimension = viewportDimension;
    context.renderSettings    = renderSettings;
    context.currentImageIndex = m_swapChain->getCurrentImageIndex();

    m_frameResources->updatePerFrameResources(context);

    return context;
  }
}

void Renderer::renderFrame(RenderContext& context) {
  CPU_ZONE_NC("Renderer::renderFrame", color::CYAN);

  if (!context.commandBuffer || !m_frameResources.get()) {
    GlobalLogger::Log(LogLevel::Error, "Invalid render context");
    return;
  }

  GPU_ZONE_NC(context.commandBuffer.get(), "Frame Render", color::CYAN);

  // TODO: divide this into separate functions

  if (m_basePass) {
    m_basePass->prepareFrame(context);
  }

  if (m_debugPass) {
    m_debugPass->prepareFrame(context);
  }

  if (m_finalPass) {
    m_finalPass->prepareFrame(context);
  }

  bool exclusiveMode
      = m_debugPass && context.renderSettings.renderMode != RenderMode::Solid && m_debugPass->isExclusive();

  if (!exclusiveMode && m_basePass) {
    m_basePass->render(context);
  }

  auto isDebugPass = context.renderSettings.renderMode == RenderMode::Wireframe
                  || context.renderSettings.renderMode == RenderMode::ShaderOverdraw
                  || context.renderSettings.renderMode == RenderMode::VertexNormalVisualization
                  || context.renderSettings.renderMode == RenderMode::NormalMapVisualization
                  || context.renderSettings.renderMode == RenderMode::LightVisualization
                  || context.renderSettings.renderMode == RenderMode::WorldGrid
                  || context.renderSettings.renderMode == RenderMode::MeshHighlight
                  || context.renderSettings.renderMode == RenderMode::BoundingBoxVisualization;

  if (m_debugPass && isDebugPass) {
    m_debugPass->render(context);
  }

  if (m_finalPass && context.renderSettings.appMode == ApplicationMode::Standalone) {
    m_finalPass->render(context);
  }

  if (m_basePass) {
    m_basePass->endFrame();
  }

  if (m_debugPass) {
    m_debugPass->endFrame();
  }

  if (m_finalPass) {
    m_finalPass->endFrame();
  }
}

void Renderer::endFrame(RenderContext& context) {
  CPU_ZONE_NC("Renderer::endFrame", color::PURPLE);
  if (!context.commandBuffer) {
    return;
  }

  {
    CPU_ZONE_NC("Collect GPU Profiler Data", color::PURPLE);
    GPU_ZONE_NC(context.commandBuffer.get(), "End Frame", color::PURPLE);

    if (auto* profiler = ServiceLocator::s_get<gpu::GpuProfiler>()) {
      profiler->collect(context.commandBuffer.get());
    }
  }

  {
    CPU_ZONE_NC("End Command Buffer", color::PURPLE);
    context.commandBuffer->end();
  }

  auto     frameManager            = ServiceLocator::s_get<FrameManager>();
  uint32_t currentFrameIndex       = frameManager->getCurrentFrameIndex();
  auto&    renderFinishedSemaphore = m_renderFinishedSemaphores[currentFrameIndex];
  {
    CPU_ZONE_NC("Prepare Semaphores", color::PURPLE);
    std::vector<rhi::Semaphore*> waitSemaphores;
    auto&                        imageAvailableSemaphore = m_imageAvailableSemaphores[currentFrameIndex];
    if (imageAvailableSemaphore.get()) {
      waitSemaphores.push_back(imageAvailableSemaphore.get());
    }

    std::vector<rhi::Semaphore*> signalSemaphores;

    if (renderFinishedSemaphore.get()) {
      signalSemaphores.push_back(renderFinishedSemaphore.get());
    }

    m_device->submitCommandBuffer(
        context.commandBuffer.get(), m_frameFences[currentFrameIndex].get(), waitSemaphores, signalSemaphores);
  }

  {
    CPU_ZONE_NC("Present", color::PURPLE);
    m_swapChain->present(renderFinishedSemaphore.get());
  }
  {
    CPU_ZONE_NC("Cleanup", color::PURPLE);
    recycleCommandBuffer_(std::move(context.commandBuffer));
  }
}

bool Renderer::onWindowResize(uint32_t width, uint32_t height) {
  waitForAllFrames_();

  if (!m_swapChain->resize(width, height)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to resize swap chain");
    return false;
  }

  m_frameResources->resize(math::Dimension2i(width, height));

  if (m_basePass) {
    m_basePass->resize(math::Dimension2i(width, height));
  }

  if (m_debugPass) {
    m_debugPass->resize(math::Dimension2i(width, height));
  }

  if (m_finalPass) {
    m_finalPass->resize(math::Dimension2i(width, height));
  }

  return true;
}

bool Renderer::onViewportResize(const math::Dimension2i& newDimension) {
  const auto& currentViewport = m_frameResources->getViewport();

  if (static_cast<int>(currentViewport.width) == newDimension.width()
      && static_cast<int>(currentViewport.height) == newDimension.height()) {
    return true;
  }

  m_device->waitIdle();

  auto width  = newDimension.width() > 0 ? newDimension.width() : 1;
  auto height = newDimension.height() > 0 ? newDimension.height() : 1;

  GlobalLogger::Log(LogLevel::Info, "Resizing viewport to " + std::to_string(width) + "x" + std::to_string(height));

  // this is not ideal solution, but it works for now
  auto scene = ServiceLocator::s_get<SceneManager>()->getCurrentScene();
  if (scene) {
    auto& registry = scene->getEntityRegistry();
    auto  view     = registry.view<Camera>();

    if (!view.empty()) {
      auto  entity  = view.front();
      auto& camera  = view.get<Camera>(entity);
      camera.width  = width;
      camera.height = height;
    }
  }

  m_frameResources->resize(math::Dimension2i(width, height));

  if (m_basePass) {
    m_basePass->resize(math::Dimension2i(width, height));
  }

  if (m_debugPass) {
    m_debugPass->resize(math::Dimension2i(width, height));
  }

  if (m_finalPass) {
    m_finalPass->resize(math::Dimension2i(width, height));
  }

  return true;
}

void Renderer::onSceneSwitch() {
  m_device->waitIdle();
  if (m_frameResources) {
    m_frameResources->clearSceneResources();
  }
  if (m_basePass) {
    m_basePass->clearSceneResources();
  }
  if (m_debugPass) {
    m_debugPass->clearSceneResources();
  }

  GlobalLogger::Log(LogLevel::Info, "Renderer resources cleared for scene switch");
}

gfx::rhi::RenderingApi Renderer::getCurrentApi() const {
  return m_device ? m_device->getApiType() : gfx::rhi::RenderingApi::Count;
}

void Renderer::updateWindow(Window* window) {
  if (!window) {
    GlobalLogger::Log(LogLevel::Error, "Cannot update with null window");
    return;
  }
  m_window = window;
  GlobalLogger::Log(LogLevel::Info, "Window pointer updated successfully");
}

bool Renderer::switchRenderingApi(gfx::rhi::RenderingApi newApi) {
  if (!m_initialized) {
    GlobalLogger::Log(LogLevel::Error, "Cannot switch API - renderer not initialized");
    return false;
  }

  if (getCurrentApi() == newApi) {
    GlobalLogger::Log(LogLevel::Info, "Already using requested API, no switch needed");
    return true;
  }

  GlobalLogger::Log(LogLevel::Info, "Starting API switch to " + std::to_string(static_cast<int>(newApi)));

  waitForAllFrames_();

  cleanupResources_();

  if (!recreateResources_(newApi)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to recreate resources with new API");
    return false;
  }

  recreateResourceManagers_();

  reloadSceneModels_();

  GlobalLogger::Log(LogLevel::Info, "Successfully switched to new rendering API");
  return true;
}

void Renderer::initializeGpuProfiler_() {
  if (auto* profiler = ServiceLocator::s_get<gpu::GpuProfiler>()) {
    if (profiler->initialize(m_device.get())) {
      GlobalLogger::Log(LogLevel::Info, "GPU profiler initialized successfully");
    } else {
      GlobalLogger::Log(LogLevel::Warning, "Failed to initialize GPU profiler");
    }
  }
}

std::unique_ptr<rhi::CommandBuffer> Renderer::acquireCommandBuffer_() {
  auto     frameManager      = ServiceLocator::s_get<FrameManager>();
  uint32_t currentFrameIndex = frameManager->getCurrentFrameIndex();
  auto&    pool              = m_commandBufferPools[currentFrameIndex];

  if (!pool.empty()) {
    auto cmdBuffer = std::move(pool.back());
    pool.pop_back();

    cmdBuffer->reset();

    return cmdBuffer;
  }

  rhi::CommandBufferDesc cmdDesc;
  return m_device->createCommandBuffer(cmdDesc);
}

void Renderer::recycleCommandBuffer_(std::unique_ptr<rhi::CommandBuffer> cmdBuffer) {
  if (cmdBuffer) {
    auto     frameManager      = ServiceLocator::s_get<FrameManager>();
    uint32_t currentFrameIndex = frameManager->getCurrentFrameIndex();
    auto&    pool              = m_commandBufferPools[currentFrameIndex];

    if (pool.size() < COMMAND_BUFFERS_PER_FRAME) {
      pool.push_back(std::move(cmdBuffer));
    }
  }
}

void Renderer::waitForAllFrames_() {
  if (m_device) {
    m_device->waitIdle();
  }
}

void Renderer::setupRenderPasses_() {
  m_basePass = std::make_unique<BasePass>();
  m_basePass->initialize(m_device.get(), getResourceManager(), m_frameResources.get(), m_shaderManager.get());

  m_debugPass = std::make_unique<DebugPass>();
  m_debugPass->initialize(m_device.get(), getResourceManager(), m_frameResources.get(), m_shaderManager.get());

  m_finalPass = std::make_unique<FinalPass>();
  m_finalPass->initialize(m_device.get(), getResourceManager(), m_frameResources.get(), m_shaderManager.get());
}

void Renderer::cleanupResources_() {
  GlobalLogger::Log(LogLevel::Info, "Cleaning up all rendering resources");

  if (m_resourceManager) {
    m_resourceManager->clear();
    m_resourceManager.reset();
  }

  if (m_finalPass) {
    m_finalPass->cleanup();
    m_finalPass.reset();
  }

  if (m_debugPass) {
    m_debugPass->cleanup();
    m_debugPass.reset();
  }
  
  if (m_basePass) {
    m_basePass->cleanup();
    m_basePass.reset();
  }

  if (m_frameResources) {
    m_frameResources->cleanup();
    m_frameResources.reset();
  }

  if (m_shaderManager) {
    m_shaderManager->release();
    m_shaderManager.reset();
  }

  ServiceLocator::s_remove<BufferManager>();
  ServiceLocator::s_remove<TextureManager>();
  ServiceLocator::s_remove<RenderModelManager>();
  ServiceLocator::s_remove<RenderMeshManager>();
  ServiceLocator::s_remove<RenderGeometryMeshManager>();
  ServiceLocator::s_remove<MaterialManager>();

  for (auto& fence : m_frameFences) {
    fence.reset();
  }

  for (auto& semaphore : m_imageAvailableSemaphores) {
    semaphore.reset();
  }

  for (auto& semaphore : m_renderFinishedSemaphores) {
    semaphore.reset();
  }

  for (auto& pool : m_commandBufferPools) {
    pool.clear();
  }

  if (m_swapChain) {
    m_swapChain.reset();
  }

  if (m_device) {
    m_device.reset();
  }

  m_initialized = false;

  GlobalLogger::Log(LogLevel::Info, "Resource cleanup completed");
}

// TODO: consider making one method (init and this method) that will have the same logic
bool Renderer::recreateResources_(gfx::rhi::RenderingApi newApi) {
  GlobalLogger::Log(LogLevel::Info, "Recreating resources with new API");

  rhi::DeviceDesc deviceDesc;
  deviceDesc.window = m_window;
  m_device          = g_createDevice(newApi, deviceDesc);

  if (!m_device) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create device with new API");
    return false;
  }

  auto     frameManager   = ServiceLocator::s_get<FrameManager>();
  uint32_t framesInFlight = frameManager->getMaxFramesInFlight();

  m_commandBufferPools.resize(framesInFlight);
  m_frameFences.resize(framesInFlight);
  m_imageAvailableSemaphores.resize(framesInFlight);
  m_renderFinishedSemaphores.resize(framesInFlight);

  rhi::SwapchainDesc swapchainDesc;
  swapchainDesc.width       = m_window->getSize().width();
  swapchainDesc.height      = m_window->getSize().height();
  swapchainDesc.format      = rhi::TextureFormat::Bgra8;
  swapchainDesc.bufferCount = framesInFlight;
  m_swapChain               = m_device->createSwapChain(swapchainDesc);

  if (!m_swapChain) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create swap chain with new API");
    return false;
  }

  for (uint32_t i = 0; i < framesInFlight; ++i) {
    rhi::FenceDesc fenceDesc;
    fenceDesc.signaled = true;
    m_frameFences[i]   = m_device->createFence(fenceDesc);

    m_imageAvailableSemaphores[i] = m_device->createSemaphore();
    m_renderFinishedSemaphores[i] = m_device->createSemaphore();

    m_commandBufferPools[i].clear();
    m_commandBufferPools[i].reserve(COMMAND_BUFFERS_PER_FRAME);
    for (uint32_t j = 0; j < INITIAL_COMMAND_BUFFERS; ++j) {
      rhi::CommandBufferDesc cmdDesc;
      auto                   cmdBuffer = m_device->createCommandBuffer(cmdDesc);
      if (cmdBuffer) {
        m_commandBufferPools[i].emplace_back(std::move(cmdBuffer));
      }
    }
  }

  m_shaderManager   = std::make_unique<rhi::ShaderManager>(m_device.get(), framesInFlight, true);
  m_resourceManager = std::make_unique<RenderResourceManager>();
  m_frameResources  = std::make_unique<FrameResources>(m_device.get(), getResourceManager());

  m_frameResources->initialize(framesInFlight);
  m_frameResources->resize(m_window->getSize());

  setupRenderPasses_();

  auto deletionManager = ServiceLocator::s_get<ResourceDeletionManager>();
  if (deletionManager) {
    deletionManager->setDefaultFrameDelay(framesInFlight);
  }

  initializeGpuProfiler_();

  m_initialized = true;

  GlobalLogger::Log(LogLevel::Info, "Resource recreation completed");
  return true;
}

void Renderer::recreateResourceManagers_() {
  GlobalLogger::Log(LogLevel::Info, "Recreating resource managers with new device");

  ServiceLocator::s_provide<BufferManager>(m_device.get());
  ServiceLocator::s_provide<TextureManager>(m_device.get());
  ServiceLocator::s_provide<RenderModelManager>();
  ServiceLocator::s_provide<RenderMeshManager>();
  ServiceLocator::s_provide<RenderGeometryMeshManager>();
  ServiceLocator::s_provide<MaterialManager>();

  auto systemManager = ServiceLocator::s_get<SystemManager>();
  if (systemManager) {
    auto existingLightSystem = systemManager->getSystem<LightSystem>();
    if (existingLightSystem) {
      systemManager->replaceSystem<LightSystem>(m_device.get(), m_resourceManager.get());
    }
  }

  GlobalLogger::Log(LogLevel::Info, "Resource managers recreated with new device");
}

void Renderer::reloadSceneModels_() {
  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  if (!sceneManager) {
    return;
  }

  auto scene = sceneManager->getCurrentScene();
  if (!scene) {
    return;
  }

  auto& registry = scene->getEntityRegistry();

  // Consider *all* entities with a CPU model. GPU model may or may not be present.
  auto viewAll = registry.view<Model*>();

  std::vector<std::pair<entt::entity, std::filesystem::path>> entitiesToUpdate;

  for (auto entity : viewAll) {
    auto* cpuModel = registry.get<Model*>(entity);
    if (cpuModel && !cpuModel->filePath.empty()) {
      entitiesToUpdate.emplace_back(entity, cpuModel->filePath);
      GlobalLogger::Log(LogLevel::Info,
                        "Entity " + std::to_string(static_cast<uint32_t>(entity))
                            + " scheduled for GPU model reload: " + cpuModel->filePath.string());
    }
  }

  // Remove any stale GPU pointers before recreating them.
  registry.clear<RenderModel*>();

  auto renderModelManager = ServiceLocator::s_get<RenderModelManager>();
  if (!renderModelManager) {
    GlobalLogger::Log(LogLevel::Error, "RenderModelManager not available for model reload");
    return;
  }

  for (const auto& [entity, modelPath] : entitiesToUpdate) {
    if (!registry.valid(entity)) {
      GlobalLogger::Log(LogLevel::Warning,
                        "Entity " + std::to_string(static_cast<uint32_t>(entity)) + " no longer valid during reload");
      continue;
    }

    Model* cpuModel       = nullptr;
    auto*  newRenderModel = renderModelManager->getRenderModel(modelPath.string(), &cpuModel);

    if (newRenderModel) {
      registry.emplace<RenderModel*>(entity, newRenderModel);

      GlobalLogger::Log(LogLevel::Info,
                        "Entity " + std::to_string(static_cast<uint32_t>(entity))
                            + " GPU model successfully reloaded: " + modelPath.string());
    } else {
      GlobalLogger::Log(LogLevel::Error,
                        "Failed to reload GPU model for entity " + std::to_string(static_cast<uint32_t>(entity)) + ": "
                            + modelPath.string());
    }
  }

  GlobalLogger::Log(LogLevel::Info, "Reloaded GPU models for " + std::to_string(entitiesToUpdate.size()) + " entities");
}

}  // namespace renderer
}  // namespace gfx
}  // namespace arise