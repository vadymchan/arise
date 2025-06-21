#include "core/engine.h"

#include "config/config_manager.h"
#include "config/runtime_settings.h"
#include "core/application.h"
#include "ecs/component_loaders.h"
#include "ecs/components/camera.h"
#include "ecs/systems/bounding_volume_system.h"
#include "ecs/systems/camera_system.h"
#include "ecs/systems/light_system.h"
#include "ecs/systems/movement_system.h"
#include "ecs/systems/render_system.h"
#include "ecs/systems/system_manager.h"
#include "event/application_event_manager.h"
#include "event/window_event_manager.h"
#include "gfx/renderer/render_resource_manager.h"
#include "gfx/renderer/renderer.h"
#include "gfx/rhi/shader_manager.h"
#include "input/input_manager.h"
#include "profiler/backends/gpu_profiler_factory.h"
#include "profiler/profiler.h"
#include "resources/cgltf/cgltf_material_loader.h"
#include "resources/cgltf/cgltf_model_loader.h"
#include "resources/cgltf/cgltf_render_model_loader.h"
#include "scene/scene_loader.h"
#include "scene/scene_manager.h"
#include "utils/asset/asset_loader.h"
#include "utils/buffer/buffer_manager.h"
#include "utils/frame_manager/frame_manager.h"
#include "utils/hot_reload/hot_reload_manager.h"
#include "utils/image/image_loader_manager.h"
#include "utils/image/image_manager.h"
#include "utils/logger/console_logger.h"
#include "utils/logger/file_logger.h"
#include "utils/logger/global_logger.h"
#include "utils/logger/memory_logger.h"
#include "utils/material/material_loader_manager.h"
#include "utils/material/material_manager.h"
#include "utils/math/math_util.h"
#include "utils/model/mesh_manager.h"
#include "utils/model/model_manager.h"
#include "utils/model/render_geometry_mesh_manager.h"
#include "utils/model/render_mesh_manager.h"
#include "utils/model/render_model_loader_manager.h"
#include "utils/model/render_model_manager.h"
#include "utils/path_manager/path_manager.h"
#include "utils/resource/resource_deletion_manager.h"
#include "utils/service/service_locator.h"
#include "utils/texture/texture_manager.h"
#include "utils/third_party/directx_tex_util.h"
#include "utils/third_party/ktx_image_loader.h"
#include "utils/third_party/stb_util.h"
#include "utils/time/stopwatch.h"
#include "utils/time/timing_manager.h"

#include <imgui_impl_sdl2.h>

namespace arise {

Engine::~Engine() {
  m_application_->release();

  if (m_renderer_) {
    m_renderer_->getDevice()->waitIdle();
  }

  ServiceLocator::s_remove<ConfigManager>();
  ServiceLocator::s_remove<FileWatcherManager>();
  ServiceLocator::s_remove<HotReloadManager>();
  ServiceLocator::s_remove<InputManager>();
  ServiceLocator::s_remove<InputContextManager>();
  ServiceLocator::s_remove<WindowEventManager>();
  ServiceLocator::s_remove<ApplicationEventManager>();
  ServiceLocator::s_remove<SceneManager>();
  ServiceLocator::s_remove<SystemManager>();
  ServiceLocator::s_remove<FrameManager>();
  ServiceLocator::s_remove<TimingManager>();
  ServiceLocator::s_remove<AssetLoader>();
  ServiceLocator::s_remove<RenderModelManager>();
  ServiceLocator::s_remove<RenderModelLoaderManager>();
  ServiceLocator::s_remove<ImageManager>();
  ServiceLocator::s_remove<ImageLoaderManager>();
  ServiceLocator::s_remove<ResourceDeletionManager>();
  ServiceLocator::s_remove<TextureManager>();
  ServiceLocator::s_remove<BufferManager>();
  ServiceLocator::s_remove<gpu::GpuProfiler>();

  GlobalLogger::Shutdown();
}

auto Engine::initialize() -> bool {
  bool successfullyInitialized{true};

  // logger
  // ------------------------------------------------------------------------
  auto consoleLogger = std::make_unique<ConsoleLogger>("console_logger");
  GlobalLogger::AddLogger(std::move(consoleLogger));

  // auto memoryLogger = std::make_unique<MemoryLogger>("memory_logger");
  // GlobalLogger::AddLogger(std::move(memoryLogger));

  // auto fileLogger = std::make_unique<FileLogger>("file_logger");
  // GlobalLogger::AddLogger(std::move(fileLogger));

  GlobalLogger::Log(LogLevel::Info, "Engine::initialize() started");

  // input event
  // ------------------------------------------------------------------------
  auto keyboardEventHandler = std::make_unique<KeyboardEventHandler>();
  auto mouseEventHandler    = std::make_unique<MouseEventHandler>();

  // window event
  // ------------------------------------------------------------------------
  auto windowEventHandler = std::make_unique<WindowEventHandler>();
  windowEventHandler->subscribe(SDL_WINDOWEVENT_RESIZED, [this](const WindowEvent& event) {
    auto renderMode = m_editor_->getRenderParams().appMode;

    auto device = m_renderer_->getDevice();
    if (device) {
      device->waitIdle();
    }

    if (renderMode == gfx::renderer::ApplicationRenderMode::Game) {
      this->m_window_->onResize(event);

      auto  scene    = ServiceLocator::s_get<SceneManager>()->getCurrentScene();
      auto& registry = scene->getEntityRegistry();
      auto  view     = registry.view<Camera>();
      auto  entity   = view.front();
      if (entity != entt::null) {
        auto& camera  = view.get<Camera>(entity);
        camera.width  = event.data1;
        camera.height = event.data2;
      }

      m_renderer_->onWindowResize(event.data1, event.data2);
    } else if (renderMode == gfx::renderer::ApplicationRenderMode::Editor) {
      this->m_window_->onResize(event);
      m_renderer_->onWindowResize(event.data1, event.data2);
      m_editor_->onWindowResize(event.data1, event.data2);

      GlobalLogger::Log(LogLevel::Info, "Window resize in editor mode - ignoring renderer resize");
    }
  });

  // application event
  // ------------------------------------------------------------------------
  auto applicationEventHandler = std::make_unique<ApplicationEventHandler>();
  applicationEventHandler->subscribe(SDL_QUIT, std::bind(&Engine::onClose, this, std::placeholders::_1));

  // asset loader
  // ------------------------------------------------------------------------
  auto assetLoader = std::make_unique<AssetLoader>();
  assetLoader->initialize();

  // service locator
  // ------------------------------------------------------------------------
  constexpr uint32_t framesInFlight = 2;

  ServiceLocator::s_provide<ConfigManager>();
  ServiceLocator::s_provide<FileWatcherManager>();
  ServiceLocator::s_provide<HotReloadManager>();
  ServiceLocator::s_provide<InputManager>(std::move(keyboardEventHandler), std::move(mouseEventHandler));
  ServiceLocator::s_provide<InputContextManager>();
  ServiceLocator::s_provide<WindowEventManager>(std::move(windowEventHandler));
  ServiceLocator::s_provide<ApplicationEventManager>(std::move(applicationEventHandler));
  ServiceLocator::s_provide<SceneManager>();
  ServiceLocator::s_provide<SystemManager>();
  ServiceLocator::s_provide<TimingManager>();
  ServiceLocator::s_provide<FrameManager>(framesInFlight);
  ServiceLocator::s_provide<ResourceDeletionManager>();
  ServiceLocator::s_provide<AssetLoader>(std::move(assetLoader));

  auto inputManager      = ServiceLocator::s_get<InputManager>();
  auto contextManagerPtr = ServiceLocator::s_get<InputContextManager>();
  inputManager->setContextManager(contextManagerPtr);

  // config
  // ------------------------------------------------------------------------
  auto configManager = ServiceLocator::s_get<ConfigManager>();
  auto configPath    = PathManager::s_getEngineSettingsPath() / "settings.json";
  configManager->addConfig(configPath);
  auto config = configManager->getConfig(configPath);

  // rendering API
  // ------------------------------------------------------------------------
  gfx::rhi::RenderingApi renderingApi;
  std::string            renderingApiString;

#ifdef ARISE_FORCE_VULKAN
  renderingApi       = gfx::rhi::RenderingApi::Vulkan;
  renderingApiString = "vulkan";
  GlobalLogger::Log(LogLevel::Info, "RHI API forced to Vulkan at compile time");
#elif defined(ARISE_FORCE_DIRECTX)
  renderingApi       = gfx::rhi::RenderingApi::Dx12;
  renderingApiString = "dx12";
  GlobalLogger::Log(LogLevel::Info, "RHI API forced to DirectX at compile time");
#else
  renderingApiString = config->get<std::string>("renderingApi");

  if (renderingApiString == "dx12") {
    renderingApi = gfx::rhi::RenderingApi::Dx12;
  } else if (renderingApiString == "vulkan") {
    renderingApi = gfx::rhi::RenderingApi::Vulkan;
  } else {
    renderingApi       = gfx::rhi::RenderingApi::Vulkan;
    renderingApiString = "vulkan";
  }

  GlobalLogger::Log(LogLevel::Info, "RHI API selected from config: " + renderingApiString);
#endif

  // application mode
  // ------------------------------------------------------------------------
  auto applicationModeStr = config->get<std::string>("applicationMode");

  // profiler
  // ------------------------------------------------------------------------
#ifdef ARISE_USE_GPU_PROFILING
  auto gpuProfiler = gpu::GpuProfilerFactory::create(renderingApi);
  if (gpuProfiler) {
    ServiceLocator::s_provide<gpu::GpuProfiler>(std::move(gpuProfiler));
    GlobalLogger::Log(LogLevel::Info, "GPU profiler created for " + renderingApiString);
  } else {
    GlobalLogger::Log(LogLevel::Warning, "Failed to create GPU profiler for " + renderingApiString);
  }
#else
  GlobalLogger::Log(LogLevel::Info, "GPU profiling disabled at compile time");
#endif

#ifdef TRACY_ENABLE
  tracy::SetThreadName("Main Thread");
#endif

  // window
  // ------------------------------------------------------------------------
  m_window_ = std::make_unique<Window>(
      renderingApiString,
      // Desired size (for maximized window will be 0)
      math::Dimension2i{0, 0},
      math::Point2i{SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED},
      arise::Window::Flags::Resizable | arise::Window::Flags::Vulkan | arise::Window::Flags::Maximized);

  // ecs
  // ------------------------------------------------------------------------
  auto systemManager = ServiceLocator::s_get<SystemManager>();
  systemManager->addSystem(std::make_unique<CameraSystem>());
  systemManager->addSystem(std::make_unique<MovementSystem>());
  systemManager->addSystem(std::make_unique<BoundingVolumeSystem>());
  systemManager->addSystem(std::make_unique<RenderSystem>());

  // renderer
  // ------------------------------------------------------------------------
  m_renderer_ = std::make_unique<gfx::renderer::Renderer>();
  m_renderer_->initialize(m_window_.get(), renderingApi);

  // These managers are depending on the renderer device
  auto device = m_renderer_->getDevice();
  ServiceLocator::s_provide<TextureManager>(device);
  ServiceLocator::s_provide<BufferManager>(device);

  systemManager->addSystem(std::make_unique<LightSystem>(device, m_renderer_->getResourceManager()));

  // image loader
  // ------------------------------------------------------------------------
  auto imageLoaderManager = std::make_unique<ImageLoaderManager>();
  auto stbLoader          = std::make_shared<STBImageLoader>();
  imageLoaderManager->registerLoader(ImageType::JPEG, stbLoader);
  imageLoaderManager->registerLoader(ImageType::JPG, stbLoader);
  imageLoaderManager->registerLoader(ImageType::PNG, stbLoader);
  imageLoaderManager->registerLoader(ImageType::BMP, stbLoader);
  imageLoaderManager->registerLoader(ImageType::TGA, stbLoader);
  imageLoaderManager->registerLoader(ImageType::GIF, stbLoader);
  imageLoaderManager->registerLoader(ImageType::HDR, stbLoader);
  imageLoaderManager->registerLoader(ImageType::PIC, stbLoader);
  imageLoaderManager->registerLoader(ImageType::PPM, stbLoader);
  imageLoaderManager->registerLoader(ImageType::PGM, stbLoader);
  auto directxTexLoader = std::make_shared<DirectXTexImageLoader>();
  imageLoaderManager->registerLoader(ImageType::DDS, directxTexLoader);
  auto khronosTexLoader = std::make_shared<KtxImageLoader>();
  imageLoaderManager->registerLoader(ImageType::KTX, khronosTexLoader);
  imageLoaderManager->registerLoader(ImageType::KTX2, khronosTexLoader);
  ServiceLocator::s_provide<ImageLoaderManager>(std::move(imageLoaderManager));

  auto imageManager = std::make_unique<ImageManager>();
  ServiceLocator::s_provide<ImageManager>(std::move(imageManager));

  // mesh / model / material loader
  // ------------------------------------------------------------------------

  // CPU
  ServiceLocator::s_provide<MeshManager>();
  auto modelLoaderManager  = std::make_unique<ModelLoaderManager>();
  auto cgltfCpuModelLoader = std::make_shared<CgltfModelLoader>();
  modelLoaderManager->registerLoader(ModelType::GLTF, cgltfCpuModelLoader);
  modelLoaderManager->registerLoader(ModelType::GLB, cgltfCpuModelLoader);
  ServiceLocator::s_provide<ModelLoaderManager>(std::move(modelLoaderManager));
  ServiceLocator::s_provide<ModelManager>();

  // GPU
  ServiceLocator::s_provide<RenderMeshManager>();
  ServiceLocator::s_provide<RenderGeometryMeshManager>();
  auto renderModelLoaderManager = std::make_unique<RenderModelLoaderManager>();
  auto cgltfModelLoader         = std::make_shared<CgltfRenderModelLoader>();
  renderModelLoaderManager->registerLoader(ModelType::GLTF, cgltfModelLoader);
  renderModelLoaderManager->registerLoader(ModelType::GLB, cgltfModelLoader);
  ServiceLocator::s_provide<RenderModelLoaderManager>(std::move(renderModelLoaderManager));
  auto renderModelManager = std::make_unique<RenderModelManager>();
  ServiceLocator::s_provide<RenderModelManager>(std::move(renderModelManager));

  // Materials
  ServiceLocator::s_provide<MaterialManager>();
  auto materialLoaderManager = std::make_unique<MaterialLoaderManager>();
  auto cgltfMaterialLoader   = std::make_shared<CgltfMaterialLoader>();
  materialLoaderManager->registerLoader(MaterialType::GLTF, cgltfMaterialLoader);
  ServiceLocator::s_provide<MaterialLoaderManager>(std::move(materialLoaderManager));
  // editor
  // ------------------------------------------------------------------------

  gfx::renderer::ApplicationRenderMode applicationMode = gfx::renderer::ApplicationRenderMode::Game;
  if (applicationModeStr == "editor") {
    m_applicationMode = gfx::renderer::ApplicationRenderMode::Editor;
  } else if (applicationModeStr == "game") {
    m_applicationMode = gfx::renderer::ApplicationRenderMode::Game;
  }

  m_editor_ = std::make_unique<Editor>();
  switch (m_applicationMode) {
    case gfx::renderer::ApplicationRenderMode::Editor:
      if (!m_editor_->initialize(m_window_.get(),
                                 renderingApi,
                                 m_renderer_->getDevice(),
                                 m_renderer_->getFrameResources(),
                                 m_renderer_.get())) {
        GlobalLogger::Log(LogLevel::Error, "Failed to initialize Editor");
        successfullyInitialized = false;
      }
      GlobalLogger::Log(LogLevel::Info, "Editor initialized successfully");
      break;
    case gfx::renderer::ApplicationRenderMode::Game:
      GlobalLogger::Log(LogLevel::Info, "Running in Application mode, Editor not initialized");
      break;
    default:
      GlobalLogger::Log(LogLevel::Error, "Unknown application mode");
      successfullyInitialized = false;
      break;
  }

  if (successfullyInitialized) {
    GlobalLogger::Log(LogLevel::Info, "Engine::initialize() completed");
  }

  return successfullyInitialized;
}

void Engine::render() {
  CPU_ZONE_NC("Engine::render", color::CYAN);
  auto windowSize = m_window_->getSize();
  if (windowSize.width() == 0 || windowSize.height() == 0 || m_window_->isMinimized()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return;
  }

  auto& renderSettings = m_editor_->getRenderParams();

  auto context = m_renderer_->beginFrame(ServiceLocator::s_get<SceneManager>()->getCurrentScene(), renderSettings);

  m_renderer_->renderFrame(context);

  m_editor_->render(context);

  m_renderer_->endFrame(context);
}

void Engine::run() {
  CPU_ZONE_NC("Engine Main Loop", color::BLACK);
  
  auto frameManager = ServiceLocator::s_get<FrameManager>();
  if (!frameManager) {
    GlobalLogger::Log(LogLevel::Error, "FrameManager not found");
    return;
  }
  
  m_isRunning_ = true;

  while (m_isRunning_) {
    PROFILE_FRAME();

    auto timingManager = ServiceLocator::s_get<TimingManager>();
    {
      CPU_ZONE_N("Timing Update");
      timingManager->update();
    }

    {
      CPU_ZONE_N("Event Processing");
      processEvents_();
    }

    {
      CPU_ZONE_N("Input Processing");
      m_application_->processInput();
    }

    {
      CPU_ZONE_N("Game Update");
      update_(timingManager->getDeltaTime());
    }

    render();

    frameManager->advanceFrame();

    PROFILE_PLOT("FPS", timingManager->getFPS());
    PROFILE_PLOT("Frame Time (ms)", timingManager->getFrameTime());
  }
}

void Engine::processEvents_() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (m_applicationMode == gfx::renderer::ApplicationRenderMode::Editor) {
      ImGui_ImplSDL2_ProcessEvent(&event);
    }
    ServiceLocator::s_get<InputManager>()->routeEvent(event);
    ServiceLocator::s_get<WindowEventManager>()->routeEvent(event);
    ServiceLocator::s_get<ApplicationEventManager>()->routeEvent(event);
  }
}

void Engine::update_(float deltaTime) {
  if (m_applicationMode == gfx::renderer::ApplicationRenderMode::Editor) {
    m_editor_->update(deltaTime);
  }

  m_application_->update(deltaTime);

  auto systemManager = ServiceLocator::s_get<SystemManager>();
  auto scene         = ServiceLocator::s_get<SceneManager>()->getCurrentScene();
  systemManager->updateSystems(scene, deltaTime);

}

void Engine::setGame(Application* game) {
  m_application_ = game;
  if (m_application_) {
    m_application_->linkWithEngine(*this);
  }
}

}  // namespace arise