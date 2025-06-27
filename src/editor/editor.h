#ifndef ARISE_EDITOR_H
#define ARISE_EDITOR_H

#include "gfx/renderer/renderer.h"
#include "gfx/rhi/common/rhi_enums.h"
#include "input/actions.h"
#include "utils/time/stopwatch.h"
#include "utils/ui/imgui_rhi_context.h"

#include <ImGuiFileDialog.h>
#include <ImGuizmo.h>

#include <functional>

namespace arise {

class Window;

namespace gfx {
namespace rhi {
class Device;
}  // namespace rhi

namespace renderer {
class Renderer;
}  // namespace renderer
}  // namespace gfx

class Editor {
  public:
  Editor()  = default;
  ~Editor() = default;

  // TODO: consider either remove renderer pointer from editor class or remove device & frame resources pointer (we can
  // get them from renderer though I'm not sure yet that it's a good approach to pass renderer in editor)
  bool initialize(Window*                        window,
                  gfx::rhi::RenderingApi         renderingApi,
                  gfx::rhi::Device*              device,
                  gfx::renderer::FrameResources* frameResources,
                  gfx::renderer::Renderer*       renderer);

  void render(gfx::renderer::RenderContext& context);

  void update(float deltaTime);

  const gfx::renderer::RenderSettings& getRenderParams() const { return m_renderParams; }

  void onWindowResize(uint32_t width, uint32_t height);

  void setWindowRecreationCallback(std::function<Window*(gfx::rhi::RenderingApi)> callback) {
    m_windowRecreationCallback = std::move(callback);
  }

  private:
  void resizeViewport(const gfx::renderer::RenderContext& context);

  void renderMainMenu();
  void renderPerformanceWindow();
  void renderViewportWindow(gfx::renderer::RenderContext& context);
  void renderModeSelectionWindow();
  void renderSceneHierarchyWindow();
  void renderInspectorWindow();
  void renderNotifications();
  void renderControlsWindow();
  void renderApiWindow();

  void renderGizmo(const math::Dimension2i& viewportSize, const ImVec2& viewportPos);
  void handleGizmoInput(EditorAction action);
  void renderGizmoControlsWindow();

  bool           isOperationAllowedForEntity(ImGuizmo::OPERATION operation);
  void           updateGizmoConstraints();
  math::Vector3f getGizmoWorldPosition();
  void           handleGizmoManipulation(const math::Matrix4f<>& modelMatrix);
  void           handleEntitySelection(entt::entity entity);

  bool             shouldRenderGizmo_();
  entt::entity     getCameraEntity_();
  void             setupImGuizmo_(const ImVec2& viewportPos, const math::Dimension2i& viewportSize);
  math::Matrix4f<> calculateEntityModelMatrix_();
  math::Matrix4f<> calculateDirectionalLightMatrix_(Registry& registry);
  bool             performGizmoManipulation_(math::Matrix4f<>& modelMatrix, entt::entity cameraEntity);

  void clearUIFocus_();

  void saveCurrentScene_();
  void setupInputHandlers_();

  // TODO: in future consider give to a user the ability to set values
  void addDirectionalLight();
  void addPointLight();
  void addSpotLight();

  void removeSelectedEntity();

  math::Vector3f getPositionInFrontOfCamera_();

  void handleAddModelDialog();
  void createModelEntity(const std::filesystem::path& modelPath, const Transform& transform);

  void renderEntityList_(Registry& registry);

  void scanAvailableScenes_();
  void createNewScene_();
  void createDefaultCamera_();
  void switchToScene_(const std::string& sceneName);
  void renderNewSceneDialog_();
  bool hasLoadingModels_() const;

  bool switchRenderingApi_(gfx::rhi::RenderingApi newApi);
  void scheduleApiSwitch_(gfx::rhi::RenderingApi newApi);
  void processPendingApiSwitch_();
  bool recreateImGuiContext_();

  // Editor state
  gfx::renderer::RenderSettings m_renderParams;

  gfx::renderer::Renderer* m_renderer = nullptr;

  Window*                        m_window         = nullptr;
  gfx::rhi::Device*              m_device         = nullptr;
  gfx::renderer::FrameResources* m_frameResources = nullptr;

  std::unique_ptr<gfx::ImGuiRHIContext> m_imguiContext;
  std::vector<ImTextureID>              m_viewportTextureIDs;

  bool m_pendingViewportResize = true;

  entt::entity m_selectedEntity = entt::null;

  bool                m_showGizmo             = true;
  ImGuizmo::OPERATION m_currentGizmoOperation = ImGuizmo::TRANSLATE;
  ImGuizmo::MODE      m_currentGizmoMode      = ImGuizmo::WORLD;

  math::Vector3f m_currentDirectionalLightGizmoPosition;
  float          gizmoDistanceFromCamera = 5.0f;

  bool        m_showSaveNotification = false;
  ElapsedTime m_notificationTimer;
  FrameTime   m_sceneSaveTimer;

  bool m_setInspectorFocus = false;

  bool                  m_openAddModelDialog = false;
  Transform             m_newModelTransform;
  std::filesystem::path m_modelPath;
  // TODO: consider using standard c++ library
  char                  m_modelPathBuffer[MAX_PATH_BUFFER_SIZE] = "";

  bool m_showControlsWindow = false;

  char m_hierarchySearchBuffer[256] = "";
  enum class SortOrder {
    None,
    Ascending,
    Descending
  };
  SortOrder m_hierarchySortOrder = SortOrder::None;

  std::vector<std::string> m_availableScenes;
  bool                     m_showNewSceneDialog      = false;
  char                     m_newSceneNameBuffer[256] = "";
  std::string              m_pendingSceneSwitch      = "";

  bool                   m_pendingApiSwitch = false;
  gfx::rhi::RenderingApi m_pendingApi;

  std::function<Window*(gfx::rhi::RenderingApi)> m_windowRecreationCallback;
};

}  // namespace arise

#endif  // ARISE_EDITOR_H