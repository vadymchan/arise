#include "editor/editor.h"

#include "config/config_manager.h"
#include "ecs/components/camera.h"
#include "ecs/components/light.h"
#include "ecs/components/render_model.h"
#include "input/input_manager.h"
#include "scene/scene_manager.h"
#include "scene/scene_saver.h"
#include "utils/model/render_model_manager.h"
#include "utils/path_manager/path_manager.h"
#include "utils/service/service_locator.h"
#include "utils/time/timing_manager.h"

#include <ImGuiFileDialog.h>

namespace game_engine {

bool Editor::initialize(Window*                        window,
                        gfx::rhi::RenderingApi         renderingApi,
                        gfx::rhi::Device*              device,
                        gfx::renderer::FrameResources* frameResources) {
  m_window         = window;
  m_device         = device;
  m_frameResources = frameResources;
  m_viewportTextureIDs.resize(frameResources->getFramesCount(), 0);

  m_imguiContext = std::make_unique<gfx::ImGuiRHIContext>();
  if (!m_imguiContext->initialize(window, device, frameResources->getFramesCount())) {
    GlobalLogger::Log(LogLevel::Error, "Failed to initialize ImGui context");
    return false;
  }

  ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

  m_showGizmo             = true;
  m_currentGizmoOperation = ImGuizmo::TRANSLATE;
  m_currentGizmoMode      = ImGuizmo::WORLD;

  m_renderParams.renderMode              = gfx::renderer::RenderMode::Solid;
  m_renderParams.postProcessMode         = gfx::renderer::PostProcessMode::None;
  m_renderParams.appMode                 = gfx::renderer::ApplicationRenderMode::Editor;
  m_renderParams.renderViewportDimension = window->getSize();

  m_notificationTimer.stop();
  m_sceneSaveTimer.stop();

  setupInputHandlers_();

  GlobalLogger::Log(LogLevel::Info, "Editor initialized successfully");
  return true;
}

void Editor::render(gfx::renderer::RenderContext& context) {
  if (!m_imguiContext) {
    return;
  }

  if (m_pendingViewportResize) {
    resizeViewport(context);
    m_pendingViewportResize = false;
  }

  uint32_t currentIndex = context.currentImageIndex;

  auto colorBufferTexture = m_frameResources->getRenderTargets(currentIndex).colorBuffer.get();

  if (colorBufferTexture->getCurrentLayoutType() != gfx::rhi::ResourceLayout::ShaderReadOnly) {
    gfx::rhi::ResourceBarrierDesc colorBarrier;
    colorBarrier.texture   = colorBufferTexture;
    colorBarrier.oldLayout = colorBufferTexture->getCurrentLayoutType();
    colorBarrier.newLayout = gfx::rhi::ResourceLayout::ShaderReadOnly;
    context.commandBuffer->resourceBarrier(colorBarrier);
  }

  if (!m_viewportTextureIDs[currentIndex]) {
    m_viewportTextureIDs[currentIndex] = m_imguiContext->createTextureID(colorBufferTexture, currentIndex);
  }

  m_imguiContext->beginFrame();

  ImGuizmo::BeginFrame();

  renderMainMenu();

  ImGui::DockSpaceOverViewport();

  renderPerformanceWindow();
  renderViewportWindow(context);
  renderModeSelectionWindow();
  renderSceneHierarchyWindow();
  renderInspectorWindow();
  renderGizmoControlsWindow();

  renderNotifications();

  auto backBufferTexture = m_frameResources->getRenderTargets(context.currentImageIndex).backBuffer;

  m_imguiContext->endFrame(
      context.commandBuffer.get(), backBufferTexture, m_window->getSize(), context.currentImageIndex);
}

void Editor::onWindowResize(uint32_t width, uint32_t height) {
  if (m_renderParams.appMode == gfx::renderer::ApplicationRenderMode::Editor) {
    if (width <= 0 || height <= 0) {
      GlobalLogger::Log(LogLevel::Error, "Invalid window dimensions");
      return;
    }

    math::Dimension2Di newDimensions(width, height);

    m_imguiContext->resize(newDimensions);

    m_pendingViewportResize = true;
  }
}

void Editor::resizeViewport(const gfx::renderer::RenderContext& context) {
  if (m_renderParams.renderViewportDimension.width() <= 0 || m_renderParams.renderViewportDimension.height() <= 0) {
    GlobalLogger::Log(LogLevel::Error, "Invalid viewport dimensions");
    return;
  }

  m_device->waitIdle();

  for (size_t i = 0; i < m_viewportTextureIDs.size(); i++) {
    if (m_viewportTextureIDs[i]) {
      m_imguiContext->releaseTextureID(m_viewportTextureIDs[i]);
      m_viewportTextureIDs[i] = 0;
    }
  }

  // We'll create new texture IDs on-demand in the render method
}

void Editor::renderMainMenu() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
        saveCurrentScene_();
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}

void Editor::renderPerformanceWindow() {
  auto timingManager = ServiceLocator::s_get<TimingManager>();
  if (!timingManager) {
    return;
  }

  ImGui::Begin("Performance");

  auto fps       = timingManager->getFPS();
  auto frameTime = timingManager->getFrameTime();

  ImGui::Text("FPS: %.1f", fps);
  ImGui::Text("Frame Time: %.1f ms", frameTime);

  // Frame time history graph
  static const int historyCount                   = 300;
  static float     frameTimeHistory[historyCount] = {0};
  static int       historyIndex                   = 0;

  frameTimeHistory[historyIndex] = frameTime;
  historyIndex                   = (historyIndex + 1) % historyCount;

  ImGui::PlotLines(
      "Frame Time (ms)", frameTimeHistory, historyCount, historyIndex, nullptr, 0.0f, FLT_MAX, ImVec2(0, 80));

  ImGui::End();
}

void Editor::renderViewportWindow(gfx::renderer::RenderContext& context) {
  ImGui::Begin("Render Window");

  ImVec2 renderWindow = ImGui::GetContentRegionAvail();

  if (renderWindow.x <= 0.0f) {
    renderWindow.x = 1.0f;
  }
  if (renderWindow.y <= 0.0f) {
    renderWindow.y = 1.0f;
  }

  math::Dimension2Di newDimension(renderWindow.x, renderWindow.y);

  bool viewportResized = (m_renderParams.renderViewportDimension.width() != static_cast<int>(renderWindow.x)
                          || m_renderParams.renderViewportDimension.height() != static_cast<int>(renderWindow.y));

  if (viewportResized) {
    m_renderParams.renderViewportDimension = newDimension;
    m_pendingViewportResize                = true;
  }

  uint32_t    currentIndex     = context.currentImageIndex;
  ImTextureID currentTextureID = m_viewportTextureIDs[currentIndex];

  ImVec2 viewportPos = ImGui::GetCursorScreenPos();

  if (currentTextureID) {
    ImGui::Image(currentTextureID, renderWindow);

    handleGizmoInput();

    renderGizmo(newDimension, viewportPos);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver()) {
      GlobalLogger::Log(LogLevel::Info, "Viewport clicked, but not over gizmo");
    }
  } else {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Render texture not available");
  }

  ImGui::End();
}

void Editor::renderModeSelectionWindow() {
  ImGui::Begin("Render Mode");

  if (ImGui::RadioButton("Solid", m_renderParams.renderMode == gfx::renderer::RenderMode::Solid)) {
    m_renderParams.renderMode = gfx::renderer::RenderMode::Solid;
  }

  if (ImGui::RadioButton("Wireframe", m_renderParams.renderMode == gfx::renderer::RenderMode::Wireframe)) {
    m_renderParams.renderMode = gfx::renderer::RenderMode::Wireframe;
  }

  if (ImGui::RadioButton("Normal Map Visualization",
                         m_renderParams.renderMode == gfx::renderer::RenderMode::NormalMapVisualization)) {
    m_renderParams.renderMode = gfx::renderer::RenderMode::NormalMapVisualization;
  }

  if (ImGui::RadioButton("Vertex Normal Visualization",
                         m_renderParams.renderMode == gfx::renderer::RenderMode::VertexNormalVisualization)) {
    m_renderParams.renderMode = gfx::renderer::RenderMode::VertexNormalVisualization;
  }

  if (ImGui::RadioButton("Shader Overdraw", m_renderParams.renderMode == gfx::renderer::RenderMode::ShaderOverdraw)) {
    m_renderParams.renderMode = gfx::renderer::RenderMode::ShaderOverdraw;
  }

  if (ImGui::RadioButton("Light Visualization",
                         m_renderParams.renderMode == gfx::renderer::RenderMode::LightVisualization)) {
    m_renderParams.renderMode = gfx::renderer::RenderMode::LightVisualization;
  }

  ImGui::End();
}

void Editor::renderSceneHierarchyWindow() {
  ImGui::Begin("Scene Hierarchy");

  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (scene) {
    auto& registry = scene->getEntityRegistry();

    ImGui::Text("Current Scene: %s", sceneManager->getCurrentSceneName().c_str());
    ImGui::Separator();

    if (ImGui::Button("Add Model")) {
      m_newModelTransform  = Transform();
      m_openAddModelDialog = true;
    }

    if (ImGui::Button("Add Directional Light")) {
      addDirectionalLight();
    }

    ImGui::SameLine();
    if (ImGui::Button("Add Point Light")) {
      addPointLight();
    }

    ImGui::SameLine();
    if (ImGui::Button("Add Spot Light")) {
      addSpotLight();
    }

    if (m_selectedEntity != entt::null) {
      ImGui::SameLine();
      if (ImGui::Button("Remove Selected")) {
        removeSelectedEntity();
      }
    }

    ImGui::Separator();

    auto entities = registry.view<entt::entity>();

    for (auto entity : entities) {
      std::string label = "Entity " + std::to_string(static_cast<uint32_t>(entity));

      if (registry.all_of<RenderModel*>(entity)) {
        auto* model = registry.get<RenderModel*>(entity);
        if (model && !model->filePath.empty()) {
          label += " (" + model->filePath.filename().string() + ")";
        }
      } else if (registry.all_of<Camera>(entity)) {
        label += " (Camera)";
      } else if (registry.all_of<Light, DirectionalLight>(entity)) {
        label += " (Directional Light)";
      } else if (registry.all_of<Light, PointLight>(entity)) {
        label += " (Point Light)";
      } else if (registry.all_of<Light, SpotLight>(entity)) {
        label += " (Spot Light)";
      }

      bool isSelected = (entity == m_selectedEntity);

      if (ImGui::Selectable(label.c_str(), isSelected)) {
        handleEntitySelection(entity);
      }
    }
  } else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No scene is currently loaded");
  }

  ImGui::End();

  handleAddModelDialog();
}

void Editor::renderInspectorWindow() {
  if (m_setInspectorFocus) {
    ImGui::SetNextWindowFocus();
    m_setInspectorFocus = false;
  }

  ImGui::Begin("Inspector");

  if (m_selectedEntity == entt::null) {
    ImGui::Text("No entity selected");
    ImGui::End();
    return;
  }

  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    ImGui::Text("No scene is currently loaded");
    ImGui::End();
    return;
  }

  auto& registry = scene->getEntityRegistry();

  if (!registry.valid(m_selectedEntity)) {
    ImGui::Text("Selected entity is no longer valid");
    m_selectedEntity = entt::null;
    ImGui::End();
    return;
  }

  ImGui::Text("Entity ID: %u", static_cast<uint32_t>(m_selectedEntity));
  ImGui::Separator();

  // Transform
  if (registry.all_of<Transform>(m_selectedEntity)) {
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto& transform = registry.get<Transform>(m_selectedEntity);

      float position[3] = {transform.translation.x(), transform.translation.y(), transform.translation.z()};
      if (ImGui::DragFloat3("Position", position, 0.1f)) {
        transform.translation.x() = position[0];
        transform.translation.y() = position[1];
        transform.translation.z() = position[2];
        transform.isDirty         = true;
      }

      float rotation[3] = {transform.rotation.x(), transform.rotation.y(), transform.rotation.z()};
      if (ImGui::DragFloat3("Rotation", rotation, 1.0f)) {
        transform.rotation.x() = rotation[0];
        transform.rotation.y() = rotation[1];
        transform.rotation.z() = rotation[2];
        transform.isDirty      = true;
      }

      float scale[3] = {transform.scale.x(), transform.scale.y(), transform.scale.z()};
      if (ImGui::DragFloat3("Scale", scale, 0.1f)) {
        transform.scale.x() = scale[0];
        transform.scale.y() = scale[1];
        transform.scale.z() = scale[2];
        transform.isDirty   = true;
      }
    }
  }

  // Camera
  if (registry.all_of<Camera>(m_selectedEntity)) {
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto& camera = registry.get<Camera>(m_selectedEntity);

      const char* typeItems[] = {"Perspective", "Orthographic"};
      int         currentType = static_cast<int>(camera.type);
      if (ImGui::Combo("Type", &currentType, typeItems, IM_ARRAYSIZE(typeItems))) {
        camera.type = static_cast<CameraType>(currentType);
      }

      if (camera.type == CameraType::Perspective) {
        float fov = camera.fov;
        if (ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.1f")) {
          camera.fov = fov;
        }
      }

      float nearClip = camera.nearClip;
      if (ImGui::DragFloat("Near Clip", &nearClip, 0.1f, 0.001f, camera.farClip - 0.001f)) {
        camera.nearClip = nearClip;
      }

      float farClip = camera.farClip;
      if (ImGui::DragFloat("Far Clip", &farClip, 1.0f, camera.nearClip + 0.001f, 10000.0f)) {
        camera.farClip = farClip;
      }

      float width = camera.width;
      if (ImGui::DragFloat("Width", &width, 1.0f, 1.0f, 10000.0f)) {
        camera.width = width;
      }

      float height = camera.height;
      if (ImGui::DragFloat("Height", &height, 1.0f, 1.0f, 10000.0f)) {
        camera.height = height;
      }
    }
  }

  // Light
  if (registry.all_of<Light>(m_selectedEntity)) {
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto& light = registry.get<Light>(m_selectedEntity);

      float color[3] = {light.color.x(), light.color.y(), light.color.z()};
      if (ImGui::ColorEdit3("Color", color)) {
        light.color.x() = color[0];
        light.color.y() = color[1];
        light.color.z() = color[2];
        light.isDirty   = true;
      }

      float intensity = light.intensity;
      if (ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 100.0f)) {
        light.intensity = intensity;
        light.isDirty   = true;
      }

      if (registry.all_of<DirectionalLight>(m_selectedEntity)) {
        auto& dirLight = registry.get<DirectionalLight>(m_selectedEntity);

        float direction[3] = {dirLight.direction.x(), dirLight.direction.y(), dirLight.direction.z()};
        if (ImGui::DragFloat3("Direction", direction, 0.1f)) {
          dirLight.direction.x() = direction[0];
          dirLight.direction.y() = direction[1];
          dirLight.direction.z() = direction[2];

          dirLight.direction.normalize();

          dirLight.isDirty = true;
        }
      } else if (registry.all_of<PointLight>(m_selectedEntity)) {
        auto& pointLight = registry.get<PointLight>(m_selectedEntity);

        float range = pointLight.range;
        if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 1000.0f)) {
          pointLight.range   = range;
          pointLight.isDirty = true;
        }
      } else if (registry.all_of<SpotLight>(m_selectedEntity)) {
        auto& spotLight = registry.get<SpotLight>(m_selectedEntity);

        float range = spotLight.range;
        if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 1000.0f)) {
          spotLight.range   = range;
          spotLight.isDirty = true;
        }

        float innerAngle = spotLight.innerConeAngle;
        if (ImGui::DragFloat("Inner Cone Angle", &innerAngle, 0.1f, 0.0f, spotLight.outerConeAngle)) {
          spotLight.innerConeAngle = innerAngle;
          spotLight.isDirty        = true;
        }

        float outerAngle = spotLight.outerConeAngle;
        if (ImGui::DragFloat("Outer Cone Angle", &outerAngle, 0.1f, spotLight.innerConeAngle, 90.0f)) {
          spotLight.outerConeAngle = outerAngle;
          spotLight.isDirty        = true;
        }
      }
    }
  }

  if (registry.all_of<RenderModel*>(m_selectedEntity)) {
    if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto* model = registry.get<RenderModel*>(m_selectedEntity);

      if (model) {
        ImGui::Text("File: %s", model->filePath.string().c_str());
        ImGui::Text("Meshes: %zu", model->renderMeshes.size());

        for (size_t i = 0; i < model->renderMeshes.size(); i++) {
          if (ImGui::TreeNode(("Mesh " + std::to_string(i)).c_str())) {
            auto* renderMesh = model->renderMeshes[i];

            if (renderMesh && renderMesh->material) {
              ImGui::Text("Material: %s", renderMesh->material->materialName.c_str());

              if (!renderMesh->material->textures.empty() && ImGui::TreeNode("Textures")) {
                for (const auto& [name, texture] : renderMesh->material->textures) {
                  ImGui::Text("%s: %p", name.c_str(), texture);
                }
                ImGui::TreePop();
              }

              if (!renderMesh->material->scalarParameters.empty() && ImGui::TreeNode("Scalar Parameters")) {
                for (auto& [name, value] : renderMesh->material->scalarParameters) {
                  if (ImGui::DragFloat(name.c_str(), &value, 0.01f)) {
                  }
                }
                ImGui::TreePop();
              }

              if (!renderMesh->material->vectorParameters.empty() && ImGui::TreeNode("Vector Parameters")) {
                for (auto& [name, value] : renderMesh->material->vectorParameters) {
                  float values[4] = {value.x(), value.y(), value.z(), value.w()};
                  if (ImGui::DragFloat4(name.c_str(), values, 0.01f)) {
                    value.x() = values[0];
                    value.y() = values[1];
                    value.z() = values[2];
                    value.w() = values[3];
                  }
                }
                ImGui::TreePop();
              }
            } else {
              ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No material assigned");
            }

            ImGui::TreePop();
          }
        }
      } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Model is null");
      }
    }
  }

  ImGui::End();
}

void Editor::renderNotifications() {
  if (m_showSaveNotification) {
    ImGui::SetNextWindowPos(ImVec2(m_window->getSize().width() / 2.0f - 100.0f, 50.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(200.0f, 0.0f), ImGuiCond_Always);

    const float maxDisplayTime = 2.0f;
    float       elapsedTime    = m_notificationTimer.elapsedTime<ElapsedTime::DurationFloat<>>();
    float       remainingTime  = maxDisplayTime - elapsedTime;
    float       alpha          = std::min(1.0f, remainingTime / 0.5f);

    if (alpha > 0.0f) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

      ImGui::Begin("##SaveNotification",
                   nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove
                       | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav);

      if (m_sceneSaveTimer.elapsedTime<FrameTime::DurationFloat<std::milli>>() > 0.0f) {
        float saveTime = m_sceneSaveTimer.elapsedTime<FrameTime::DurationFloat<std::milli>>();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Scene saved successfully!");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Save time: %.2f ms", saveTime);
      } else {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Scene saved successfully!");
      }

      ImGui::End();
      ImGui::PopStyleVar();
    } else {
      m_showSaveNotification = false;
    }

    if (elapsedTime >= maxDisplayTime) {
      m_showSaveNotification = false;
    }
  }
}
void Editor::renderGizmo(const math::Dimension2Di& viewportSize, const ImVec2& viewportPos) {
  if (!shouldRenderGizmo_()) {
    return;
  }

  updateGizmoConstraints();

  entt::entity cameraEntity = getCameraEntity_();
  if (cameraEntity == entt::null) {
    return;
  }

  setupImGuizmo_(viewportPos, viewportSize);

  math::Matrix4f<> modelMatrix = calculateEntityModelMatrix_();

  bool manipulated = performGizmoManipulation_(modelMatrix, cameraEntity);

  if (manipulated) {
    handleGizmoManipulation(modelMatrix);
  }
}

void Editor::handleGizmoInput() {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureKeyboard) {
    return;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_1)) {
    if (isOperationAllowedForEntity(ImGuizmo::TRANSLATE)) {
      m_currentGizmoOperation = ImGuizmo::TRANSLATE;
      GlobalLogger::Log(LogLevel::Info, "Gizmo: Switched to Translate mode");
    } else {
      GlobalLogger::Log(LogLevel::Info, "Translate operation not allowed for this entity type");
    }
  } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
    if (isOperationAllowedForEntity(ImGuizmo::ROTATE)) {
      m_currentGizmoOperation = ImGuizmo::ROTATE;
      GlobalLogger::Log(LogLevel::Info, "Gizmo: Switched to Rotate mode");
    } else {
      GlobalLogger::Log(LogLevel::Info, "Rotate operation not allowed for this entity type");
    }
  } else if (ImGui::IsKeyPressed(ImGuiKey_3)) {
    if (isOperationAllowedForEntity(ImGuizmo::SCALE)) {
      m_currentGizmoOperation = ImGuizmo::SCALE;
      GlobalLogger::Log(LogLevel::Info, "Gizmo: Switched to Scale mode");
    } else {
      GlobalLogger::Log(LogLevel::Info, "Scale operation not allowed for this entity type");
    }
  }

  if (ImGui::IsKeyPressed(ImGuiKey_4)) {
    m_currentGizmoMode = (m_currentGizmoMode == ImGuizmo::WORLD) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    GlobalLogger::Log(
        LogLevel::Info,
        m_currentGizmoMode == ImGuizmo::WORLD ? "Gizmo: Switched to World space" : "Gizmo: Switched to Local space");
  }

  if (ImGui::IsKeyPressed(ImGuiKey_5)) {
    m_showGizmo = !m_showGizmo;
    GlobalLogger::Log(LogLevel::Info, m_showGizmo ? "Gizmo: Enabled" : "Gizmo: Disabled");
  }
}

void Editor::renderGizmoControlsWindow() {
  ImGui::Begin("Gizmo Controls");

  if (m_selectedEntity == entt::null) {
    ImGui::Text("No entity selected");
    ImGui::End();
    return;
  }

  auto* sceneManager = ServiceLocator::s_get<SceneManager>();
  auto* scene        = sceneManager->getCurrentScene();

  if (!scene) {
    ImGui::End();
    return;
  }

  auto& registry = scene->getEntityRegistry();

  ImGui::Checkbox("Show Gizmo", &m_showGizmo);

  bool canTranslate = isOperationAllowedForEntity(ImGuizmo::TRANSLATE);
  bool canRotate    = isOperationAllowedForEntity(ImGuizmo::ROTATE);
  bool canScale     = isOperationAllowedForEntity(ImGuizmo::SCALE);

  if (canTranslate) {
    ImGui::SameLine();
    if (ImGui::Button("T")) {
      m_currentGizmoOperation = ImGuizmo::TRANSLATE;
    }
  } else {
    ImGui::SameLine();
    ImGui::BeginDisabled();
    ImGui::Button("T");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Translation not allowed for this entity type");
    }
  }

  if (canRotate) {
    ImGui::SameLine();
    if (ImGui::Button("R")) {
      m_currentGizmoOperation = ImGuizmo::ROTATE;
    }
  } else {
    ImGui::SameLine();
    ImGui::BeginDisabled();
    ImGui::Button("R");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Rotation not allowed for this entity type");
    }
  }

  if (canScale) {
    ImGui::SameLine();
    if (ImGui::Button("S")) {
      m_currentGizmoOperation = ImGuizmo::SCALE;
    }
  } else {
    ImGui::SameLine();
    ImGui::BeginDisabled();
    ImGui::Button("S");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Scaling not allowed for this entity type");
    }
  }

  const char* modes[]      = {"World", "Local"};
  int         current_mode = (m_currentGizmoMode == ImGuizmo::WORLD) ? 0 : 1;
  if (ImGui::Combo("Space", &current_mode, modes, IM_ARRAYSIZE(modes))) {
    m_currentGizmoMode = (current_mode == 0) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
  }

  ImGui::Separator();
  ImGui::Text("Selected Entity: %u", static_cast<uint32_t>(m_selectedEntity));

  if (registry.all_of<Light, DirectionalLight>(m_selectedEntity)) {
    ImGui::Text("Type: Directional Light");
    ImGui::Text("Allowed Operations: Rotation only");
  } else if (registry.all_of<Light, PointLight>(m_selectedEntity)) {
    ImGui::Text("Type: Point Light");
    ImGui::Text("Allowed Operations: Translation, Rotation");
  } else if (registry.all_of<Light, SpotLight>(m_selectedEntity)) {
    ImGui::Text("Type: Spot Light");
    ImGui::Text("Allowed Operations: Translation, Rotation");
  } else {
    ImGui::Text("Type: Standard Entity");
    ImGui::Text("Allowed Operations: All");
  }

  ImGui::Separator();
  ImGui::Text("Keyboard Shortcuts:");

  ImGui::BulletText("1: Translate (if allowed)");
  ImGui::BulletText("2: Rotate (if allowed)");
  ImGui::BulletText("3: Scale (if allowed)");
  ImGui::BulletText("4: Toggle World/Local");
  ImGui::BulletText("5: Toggle Visibility");

  ImGui::End();
}

bool Editor::isOperationAllowedForEntity(ImGuizmo::OPERATION operation) {
  if (m_selectedEntity == entt::null) {
    return false;
  }

  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    return false;
  }

  auto& registry = scene->getEntityRegistry();

  if (registry.all_of<Light, DirectionalLight>(m_selectedEntity)) {
    return operation == ImGuizmo::ROTATE;
  }

  if (registry.all_of<Light, PointLight>(m_selectedEntity) || registry.all_of<Light, SpotLight>(m_selectedEntity)) {
    return operation == ImGuizmo::TRANSLATE || operation == ImGuizmo::ROTATE;
  }

  return true;
}

void Editor::updateGizmoConstraints() {
  if (!isOperationAllowedForEntity(m_currentGizmoOperation)) {
    if (isOperationAllowedForEntity(ImGuizmo::TRANSLATE)) {
      m_currentGizmoOperation = ImGuizmo::TRANSLATE;
    } else if (isOperationAllowedForEntity(ImGuizmo::ROTATE)) {
      m_currentGizmoOperation = ImGuizmo::ROTATE;
    } else if (isOperationAllowedForEntity(ImGuizmo::SCALE)) {
      m_currentGizmoOperation = ImGuizmo::SCALE;
    }
  }
}

math::Vector3Df Editor::getGizmoWorldPosition() {
  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    return math::Vector3Df(0.0f, 0.0f, 0.0f);
  }

  auto& registry = scene->getEntityRegistry();

  if (registry.all_of<Light, DirectionalLight>(m_selectedEntity)) {
    return m_currentDirectionalLightGizmoPosition;
  }

  if (registry.all_of<Transform>(m_selectedEntity)) {
    auto& transform = registry.get<Transform>(m_selectedEntity);
    return transform.translation;
  }

  return math::Vector3Df(0.0f, 0.0f, 0.0f);
}

void Editor::handleGizmoManipulation(const math::Matrix4f<>& modelMatrix) {
  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    return;
  }

  auto& registry = scene->getEntityRegistry();

  math::Vector3Df translation, rotation, scale;
  float           translationArray[3], rotationArray[3], scaleArray[3];

  ImGuizmo::DecomposeMatrixToComponents(
      const_cast<float*>(modelMatrix.data()), translationArray, rotationArray, scaleArray);

  for (int i = 0; i < 3; i++) {
    translation(i) = translationArray[i];
    rotation(i)    = rotationArray[i];
    scale(i)       = scaleArray[i];
  }

  if (registry.all_of<Light, DirectionalLight>(m_selectedEntity)) {
    auto& dirLight = registry.get<DirectionalLight>(m_selectedEntity);

    math::Quaternionf quat = math::Quaternionf::fromEulerAngles(math::g_degreeToRadian(rotation.x()),
                                                                math::g_degreeToRadian(rotation.y()),
                                                                math::g_degreeToRadian(rotation.z()),
                                                                math::EulerRotationOrder::XYZ);

    math::Vector3Df forward(0.0f, 0.0f, 1.0f);
    dirLight.direction = quat.rotateVector(forward);
    dirLight.isDirty   = true;

    GlobalLogger::Log(
        LogLevel::Info,
        "DirectionalLight direction updated for entity: " + std::to_string(static_cast<uint32_t>(m_selectedEntity)));
  }

  else if (registry.all_of<Transform>(m_selectedEntity)) {
    auto& transform = registry.get<Transform>(m_selectedEntity);

    if (registry.all_of<Light, PointLight>(m_selectedEntity) || registry.all_of<Light, SpotLight>(m_selectedEntity)) {
      transform.translation = translation;
      transform.rotation    = rotation;
    }

    else {
      transform.translation = translation;
      transform.rotation    = rotation;
      transform.scale       = scale;
    }

    transform.isDirty = true;

    GlobalLogger::Log(LogLevel::Info,
                      "Transform updated for entity: " + std::to_string(static_cast<uint32_t>(m_selectedEntity)));
  }
}

void Editor::handleEntitySelection(entt::entity entity) {
  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    return;
  }

  auto& registry = scene->getEntityRegistry();

  if (registry.valid(entity) && registry.all_of<Light, DirectionalLight>(entity)) {
    auto cameraView = registry.view<Camera, CameraMatrices, Transform>();
    if (cameraView.begin() != cameraView.end()) {
      auto        cameraEntity    = *cameraView.begin();
      const auto& cameraTransform = registry.get<Transform>(cameraEntity);
      const auto& cameraMatrices  = registry.get<CameraMatrices>(cameraEntity);

      math::Vector3Df cameraForward = cameraMatrices.view.getColumn<2>().resizedCopy<3>();
      cameraForward.normalize();

      m_currentDirectionalLightGizmoPosition = getPositionInFrontOfCamera_();
    }
  }

  m_selectedEntity = entity;
}

bool Editor::shouldRenderGizmo_() {
  if (!m_showGizmo || m_selectedEntity == entt::null || !m_frameResources) {
    return false;
  }

  auto* sceneManager = ServiceLocator::s_get<SceneManager>();
  auto* scene        = sceneManager->getCurrentScene();

  if (!scene) {
    return false;
  }

  auto& registry = scene->getEntityRegistry();

  bool isDirectionalLight = registry.all_of<Light, DirectionalLight>(m_selectedEntity);
  bool hasTransform       = registry.all_of<Transform>(m_selectedEntity);

  if (!isDirectionalLight && !hasTransform) {
    return false;
  }

  return true;
}

entt::entity Editor::getCameraEntity_() {
  auto* sceneManager = ServiceLocator::s_get<SceneManager>();
  auto* scene        = sceneManager->getCurrentScene();
  if (!scene) {
    return entt::null;
  }

  auto& registry   = scene->getEntityRegistry();
  auto  cameraView = registry.view<Camera, CameraMatrices>();

  if (cameraView.begin() == cameraView.end()) {
    return entt::null;
  }

  return cameraView.front();
}

void Editor::setupImGuizmo_(const ImVec2& viewportPos, const math::Dimension2Di& viewportSize) {
  ImGuizmo::SetOrthographic(false);
  ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
  ImGuizmo::SetRect(viewportPos.x,
                    viewportPos.y,
                    static_cast<float>(viewportSize.width()),
                    static_cast<float>(viewportSize.height()));
}

math::Matrix4f<> Editor::calculateEntityModelMatrix_() {
  auto* sceneManager = ServiceLocator::s_get<SceneManager>();
  auto* scene        = sceneManager->getCurrentScene();
  auto& registry     = scene->getEntityRegistry();

  math::Matrix4f<> modelMatrix = math::Matrix4f<>::Identity();

  bool hasTransform       = registry.all_of<Transform>(m_selectedEntity);
  bool isDirectionalLight = registry.all_of<Light, DirectionalLight>(m_selectedEntity);

  if (hasTransform) {
    auto& transform = registry.get<Transform>(m_selectedEntity);
    modelMatrix     = calculateTransformMatrix(transform);
  } else if (isDirectionalLight) {
    modelMatrix = calculateDirectionalLightMatrix_(registry);
  }

  return modelMatrix;
}

math::Matrix4f<> Editor::calculateDirectionalLightMatrix_(Registry& registry) {
  auto& dirLight = registry.get<DirectionalLight>(m_selectedEntity);

  math::Vector3Df gizmoPosition = getGizmoWorldPosition();

  math::Vector3Df direction = dirLight.direction;
  direction.normalize();

  math::Vector3Df forward(0.0f, 0.0f, 1.0f);

  math::Quaternionf quat = math::Quaternionf::fromVectors(forward, direction);

  math::Matrix3f<> rotMat = quat.toRotationMatrix();

  math::Matrix4f<> modelMatrix = math::Matrix4f<>::Identity();

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      modelMatrix(i, j) = rotMat(i, j);
    }
  }

  math::g_addTranslate(modelMatrix, gizmoPosition);

  return modelMatrix;
}

bool Editor::performGizmoManipulation_(math::Matrix4f<>& modelMatrix, entt::entity cameraEntity) {
  auto* sceneManager = ServiceLocator::s_get<SceneManager>();
  auto* scene        = sceneManager->getCurrentScene();
  auto& registry     = scene->getEntityRegistry();

  const auto& cameraMatrices = registry.get<CameraMatrices>(cameraEntity);

  float* viewMatrix      = const_cast<float*>(cameraMatrices.view.data());
  float* projMatrix      = const_cast<float*>(cameraMatrices.projection.data());
  float* modelMatrix_ptr = const_cast<float*>(modelMatrix.data());

  return ImGuizmo::Manipulate(viewMatrix, projMatrix, m_currentGizmoOperation, m_currentGizmoMode, modelMatrix_ptr);
}

void Editor::saveCurrentScene_() {
  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    GlobalLogger::Log(LogLevel::Error, "No scene is currently loaded to save");
    return;
  }

  m_sceneSaveTimer.reset();
  m_sceneSaveTimer.start();

  std::string sceneName = sceneManager->getCurrentSceneName();
  if (sceneName.empty()) {
    sceneName = "untitled_scene";
  }

  auto                  pathManager = ServiceLocator::s_get<PathManager>();
  std::filesystem::path scenesDir   = pathManager->s_getScenesPath();
  std::filesystem::path filePath    = scenesDir / (sceneName + ".json");

  bool success = SceneSaver::saveScene(scene, sceneName, filePath);

  m_sceneSaveTimer.pause();

  if (success) {
    GlobalLogger::Log(LogLevel::Info,
                      "Scene saved in "
                          + std::to_string(m_sceneSaveTimer.elapsedTime<FrameTime::DurationFloat<std::milli>>())
                          + " ms");

    m_showSaveNotification = true;
    m_notificationTimer.reset();
    m_notificationTimer.start();
  }
}

void Editor::setupInputHandlers_() {
  auto keyboardEventHandler = ServiceLocator::s_get<InputManager>()->getKeyboardHandler();

  keyboardEventHandler->subscribe({SDL_KEYDOWN, SDL_SCANCODE_S}, [this](const KeyboardEvent& event) {
    if ((SDL_GetModState() & KMOD_CTRL) == 0) {
      return false;
    }

    saveCurrentScene_();
    return true;
  });

  keyboardEventHandler->subscribe({SDL_KEYDOWN, SDL_SCANCODE_I}, [this](const KeyboardEvent& event) {
    m_setInspectorFocus = true;
    GlobalLogger::Log(LogLevel::Info, "Inspector window focused");
    return true;
  });
}

void Editor::addDirectionalLight() {
  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    GlobalLogger::Log(LogLevel::Error, "No scene is currently loaded");
    return;
  }

  auto& registry = scene->getEntityRegistry();

  entt::entity entity = registry.create();

  auto& light     = registry.emplace<Light>(entity);
  light.color     = math::Vector3Df(1.0f, 1.0f, 1.0f);
  light.intensity = 1.0f;

  auto& dirLight     = registry.emplace<DirectionalLight>(entity);
  dirLight.direction = math::Vector3Df(0.0f, -1.0f, 0.0f);

  handleEntitySelection(entity);

  GlobalLogger::Log(LogLevel::Info, "Directional light added");
}

void Editor::addPointLight() {
  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    GlobalLogger::Log(LogLevel::Error, "No scene is currently loaded");
    return;
  }

  auto& registry = scene->getEntityRegistry();

  entt::entity entity = registry.create();

  auto& transform       = registry.emplace<Transform>(entity);
  transform.translation = getPositionInFrontOfCamera_();
  transform.rotation    = math::Vector3Df(0.0f, 0.0f, 0.0f);
  transform.scale       = math::Vector3Df(1.0f, 1.0f, 1.0f);

  auto& light     = registry.emplace<Light>(entity);
  light.color     = math::Vector3Df(1.0f, 1.0f, 1.0f);
  light.intensity = 1.0f;

  auto& pointLight = registry.emplace<PointLight>(entity);
  pointLight.range = 10.0f;

  handleEntitySelection(entity);

  GlobalLogger::Log(LogLevel::Info, "Point light added");
}

void Editor::addSpotLight() {
  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    GlobalLogger::Log(LogLevel::Error, "No scene is currently loaded");
    return;
  }

  auto& registry = scene->getEntityRegistry();

  entt::entity entity = registry.create();

  auto& transform       = registry.emplace<Transform>(entity);
  transform.translation = getPositionInFrontOfCamera_();
  transform.rotation    = math::Vector3Df(0.0f, 0.0f, 0.0f);
  transform.scale       = math::Vector3Df(1.0f, 1.0f, 1.0f);

  auto& light     = registry.emplace<Light>(entity);
  light.color     = math::Vector3Df(1.0f, 1.0f, 1.0f);
  light.intensity = 1.0f;

  auto& spotLight          = registry.emplace<SpotLight>(entity);
  spotLight.range          = 10.0f;
  spotLight.innerConeAngle = 15.0f;
  spotLight.outerConeAngle = 30.0f;

  handleEntitySelection(entity);

  GlobalLogger::Log(LogLevel::Info, "Spot light added");
}

void Editor::removeSelectedEntity() {
  if (m_selectedEntity == entt::null) {
    return;
  }

  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    return;
  }

  auto& registry = scene->getEntityRegistry();

  if (!registry.valid(m_selectedEntity)) {
    m_selectedEntity = entt::null;
    return;
  }

  std::string entityType = "Entity";
  if (registry.all_of<Light, DirectionalLight>(m_selectedEntity)) {
    entityType = "Directional Light";
  } else if (registry.all_of<Light, PointLight>(m_selectedEntity)) {
    entityType = "Point Light";
  } else if (registry.all_of<Light, SpotLight>(m_selectedEntity)) {
    entityType = "Spot Light";
  } else if (registry.all_of<Camera>(m_selectedEntity)) {
    entityType = "Camera";
  }

  if (registry.all_of<RenderModel*>(m_selectedEntity)) {
    RenderModel* model = registry.get<RenderModel*>(m_selectedEntity);

    size_t referenceCount = 0;
    registry.view<RenderModel*>().each([&](auto entity, RenderModel* entityModel) {
      if (entityModel == model) {
        referenceCount++;
      }
    });

    if (referenceCount == 1) {
      auto renderModelManager = ServiceLocator::s_get<RenderModelManager>();
      if (renderModelManager && model) {
        GlobalLogger::Log(
            LogLevel::Info,
            "This is the last reference to render model. Cleaning up resources for: " + model->filePath.string());
        renderModelManager->removeRenderModel(model);
      }
    } else {
      GlobalLogger::Log(LogLevel::Info,
                        "Found " + std::to_string(referenceCount - 1)
                            + " other entities using the same render model. Keeping resources.");
    }

    if (!entityType.empty()) {
      entityType += " and ";
    }

    entityType = "Model";
  }

  registry.destroy(m_selectedEntity);
  GlobalLogger::Log(LogLevel::Info, entityType + " removed");

  m_selectedEntity = entt::null;
}

math::Vector3Df Editor::getPositionInFrontOfCamera_() {
  math::Vector3Df position(0.0f, 0.0f, 0.0f);

  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    GlobalLogger::Log(LogLevel::Warning, "No scene loaded when trying to position in front of camera");
    return position;
  }

  auto& registry = scene->getEntityRegistry();

  auto cameraView = registry.view<Camera, CameraMatrices, Transform>();
  if (cameraView.begin() == cameraView.end()) {
    GlobalLogger::Log(LogLevel::Warning, "No camera found for positioning");
    return position;
  }

  auto        cameraEntity    = *cameraView.begin();
  const auto& cameraTransform = registry.get<Transform>(cameraEntity);
  const auto& cameraMatrices  = registry.get<CameraMatrices>(cameraEntity);

  math::Vector3Df cameraForward = cameraMatrices.view.getColumn<2>().resizedCopy<3>();
  cameraForward.normalize();

  position = cameraTransform.translation + (cameraForward * gizmoDistanceFromCamera);

  return position;
}

void Editor::handleAddModelDialog() {
  if (!m_openAddModelDialog) {
    return;
  }

  auto                  pathManager = ServiceLocator::s_get<PathManager>();
  std::filesystem::path modelsDir   = pathManager->s_getModelPath();

  if (m_modelPathBuffer[0] == '\0' && !m_modelPath.empty()) {
    if (m_modelPath.is_absolute()) {
      std::error_code ec;
      auto            absModelPath = std::filesystem::weakly_canonical(m_modelPath, ec);
      auto            absModelDir  = std::filesystem::weakly_canonical(modelsDir, ec);

      if (!ec && absModelPath.string().find(absModelDir.string()) == 0) {
        std::filesystem::path relativePath = std::filesystem::relative(absModelPath, absModelDir, ec);
        if (!ec) {
          strncpy(m_modelPathBuffer, relativePath.string().c_str(), MAX_PATH_BUFFER_SIZE - 1);
        } else {
          strncpy(m_modelPathBuffer, m_modelPath.string().c_str(), MAX_PATH_BUFFER_SIZE - 1);
        }
      } else {
        strncpy(m_modelPathBuffer, m_modelPath.string().c_str(), MAX_PATH_BUFFER_SIZE - 1);
      }
    } else {
      strncpy(m_modelPathBuffer, m_modelPath.string().c_str(), MAX_PATH_BUFFER_SIZE - 1);
    }

    m_modelPathBuffer[MAX_PATH_BUFFER_SIZE - 1] = '\0';
  }

  ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Add Model", &m_openAddModelDialog)) {
    ImGui::Text("Model Path:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 40.0f);

    if (ImGui::InputText("##ModelPath", m_modelPathBuffer, MAX_PATH_BUFFER_SIZE)) {
      std::filesystem::path inputPath(m_modelPathBuffer);

      if (inputPath.is_absolute()) {
        std::error_code ec;
        auto            absInputPath = std::filesystem::weakly_canonical(inputPath, ec);
        auto            absModelDir  = std::filesystem::weakly_canonical(modelsDir, ec);

        if (!ec && absInputPath.string().find(absModelDir.string()) == 0) {
          m_modelPath = std::filesystem::relative(absInputPath, absModelDir, ec);
          if (ec) {
            m_modelPath = inputPath;
          }
        } else {
          m_modelPath = inputPath;
        }
      } else {
        m_modelPath = inputPath;
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("...")) {
      IGFD::FileDialogConfig config;
      config.path              = modelsDir.string();
      config.countSelectionMax = 1;
      config.flags             = ImGuiFileDialogFlags_Modal;
      ImGuiFileDialog::Instance()->OpenDialog("ModelFileBrowser", "Select Model", ".gltf,.glb", config);
    }

    bool        pathValid = true;
    std::string warningMessage;

    if (!m_modelPath.empty()) {
      std::filesystem::path fullPath;

      if (m_modelPath.is_relative()) {
        fullPath = modelsDir / m_modelPath;
      } else {
        fullPath = m_modelPath;
      }

      std::error_code ec;

      if (std::filesystem::exists(fullPath, ec)) {
        auto absSelected = std::filesystem::weakly_canonical(fullPath, ec);
        auto absRoot     = std::filesystem::weakly_canonical(modelsDir, ec);

        if (!ec && absSelected.string().find(absRoot.string()) != 0) {
          pathValid      = false;
          warningMessage = "Warning: Path should be within " + modelsDir.string() + " directory";
        }
      } else {
        pathValid      = false;
        warningMessage = "Warning: File does not exist";
      }
    }

    if (!pathValid) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s", warningMessage.c_str());
    }

    ImGui::Separator();

    ImGui::Text("Transform:");

    float position[3] = {
      m_newModelTransform.translation.x(), m_newModelTransform.translation.y(), m_newModelTransform.translation.z()};
    if (ImGui::DragFloat3("Position", position, 0.1f)) {
      m_newModelTransform.translation.x() = position[0];
      m_newModelTransform.translation.y() = position[1];
      m_newModelTransform.translation.z() = position[2];
    }

    float rotation[3]
        = {m_newModelTransform.rotation.x(), m_newModelTransform.rotation.y(), m_newModelTransform.rotation.z()};
    if (ImGui::DragFloat3("Rotation", rotation, 1.0f)) {
      m_newModelTransform.rotation.x() = rotation[0];
      m_newModelTransform.rotation.y() = rotation[1];
      m_newModelTransform.rotation.z() = rotation[2];
    }

    float scale[3] = {m_newModelTransform.scale.x(), m_newModelTransform.scale.y(), m_newModelTransform.scale.z()};
    if (ImGui::DragFloat3("Scale", scale, 0.1f)) {
      m_newModelTransform.scale.x() = scale[0];
      m_newModelTransform.scale.y() = scale[1];
      m_newModelTransform.scale.z() = scale[2];
    }

    if (ImGui::Button("Reset Transform")) {
      m_newModelTransform       = Transform();
      m_newModelTransform.scale = math::Vector3Df(1.0f, 1.0f, 1.0f);
    }

    ImGui::Separator();

    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 160);
    if (ImGui::Button("Cancel", ImVec2(70, 0))) {
      m_openAddModelDialog = false;
    }

    ImGui::SameLine();
    bool addButtonEnabled = (!m_modelPath.empty() && pathValid);
    if (!addButtonEnabled) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Button("Add", ImVec2(70, 0)) && addButtonEnabled) {
      std::filesystem::path fullPath = m_modelPath;
      if (m_modelPath.is_relative()) {
        fullPath = modelsDir / m_modelPath;
      }

      createModelEntity(fullPath, m_newModelTransform);
      GlobalLogger::Log(LogLevel::Info, "Model added: " + fullPath.string());
      m_openAddModelDialog = false;
    }

    if (!addButtonEnabled) {
      ImGui::EndDisabled();
    }

    if (ImGuiFileDialog::Instance()->Display("ModelFileBrowser")) {
      if (ImGuiFileDialog::Instance()->IsOk()) {
        std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();

        std::filesystem::path selectedPath = filePathName;
        std::error_code       ec;

        auto absSelected = std::filesystem::weakly_canonical(selectedPath, ec);
        auto absRoot     = std::filesystem::weakly_canonical(modelsDir, ec);

        if (!ec && absSelected.string().find(absRoot.string()) == 0) {
          std::filesystem::path relativePath = std::filesystem::relative(absSelected, absRoot, ec);
          if (!ec) {
            m_modelPath = relativePath;

            strncpy(m_modelPathBuffer, m_modelPath.string().c_str(), MAX_PATH_BUFFER_SIZE - 1);
            m_modelPathBuffer[MAX_PATH_BUFFER_SIZE - 1] = '\0';
          } else {
            m_modelPath = selectedPath;
            strncpy(m_modelPathBuffer, m_modelPath.string().c_str(), MAX_PATH_BUFFER_SIZE - 1);
            m_modelPathBuffer[MAX_PATH_BUFFER_SIZE - 1] = '\0';
          }
        } else {
          m_modelPath = selectedPath;

          strncpy(m_modelPathBuffer, m_modelPath.string().c_str(), MAX_PATH_BUFFER_SIZE - 1);
          m_modelPathBuffer[MAX_PATH_BUFFER_SIZE - 1] = '\0';

          GlobalLogger::Log(LogLevel::Warning, "Selected model path is outside " + modelsDir.string() + " directory.");
        }
      }
      ImGuiFileDialog::Instance()->Close();
    }
  }
  ImGui::End();
}

void Editor::createModelEntity(const std::filesystem::path& modelPath, const Transform& transform) {
  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  auto scene        = sceneManager->getCurrentScene();

  if (!scene) {
    GlobalLogger::Log(LogLevel::Error, "No scene is currently loaded");
    return;
  }

  auto& registry = scene->getEntityRegistry();

  entt::entity entity = registry.create();

  registry.emplace<Transform>(entity, transform);

  auto modelManager = ServiceLocator::s_get<RenderModelManager>();
  if (modelManager) {
    auto renderModel = modelManager->getRenderModel(modelPath.string());

    if (renderModel) {
      registry.emplace<RenderModel*>(entity, renderModel);
      handleEntitySelection(entity);
    } else {
      GlobalLogger::Log(LogLevel::Error, "Failed to load model: " + modelPath.string());
      registry.destroy(entity);
    }
  } else {
    GlobalLogger::Log(LogLevel::Error, "RenderModelManager not available");
    registry.destroy(entity);
  }
}
}  // namespace game_engine