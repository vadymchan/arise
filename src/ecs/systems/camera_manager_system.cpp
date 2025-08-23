#include "ecs/systems/camera_manager_system.h"

#include "ecs/components/active_camera.h"
#include "ecs/components/camera.h"
#include "ecs/components/input_components.h"
#include "ecs/components/movement.h"
#include "ecs/components/transform.h"
#include "ecs/components/viewport_tag.h"
#include "utils/logger/log.h"

#include <math_library/graphics.h>

#include <sstream>

namespace arise {
namespace ecs {

entt::entity CameraManagerSystem::findActiveCamera(Registry& registry) {
  auto activeView = registry.view<Transform, Camera, CameraMatrices, ActiveCamera>();
  if (activeView.begin() != activeView.end()) {
    return *activeView.begin();
  }

  auto fallbackView = registry.view<Transform, Camera, CameraMatrices>();
  if (fallbackView.begin() != fallbackView.end()) {
    auto entity = *fallbackView.begin();
    LOG_WARN("No active camera found, using first available camera as fallback");
    registry.emplace<ActiveCamera>(entity);
    return entity;
  }

  return entt::null;
}

std::vector<entt::entity> CameraManagerSystem::getAllCameras(Registry& registry) {
  std::vector<entt::entity> cameras;
  auto view = registry.view<Transform, Camera>();
  
  for (auto entity : view) {
    cameras.push_back(entity);
  }
  
  return cameras;
}

void CameraManagerSystem::setActiveCamera(Registry& registry, entt::entity camera) {
  if (!registry.valid(camera) || !registry.all_of<Camera>(camera)) {
    LOG_ERROR("Invalid camera entity provided to setActiveCamera");
    return;
  }

  auto activeView = registry.view<ActiveCamera>();
  for (auto entity : activeView) {
    registry.remove<ActiveCamera>(entity);
  }

  registry.emplace<ActiveCamera>(camera);

  if (!registry.all_of<InputActions>(camera)) {
    registry.emplace<InputActions>(camera);
  }
  if (!registry.all_of<MouseInput>(camera)) {
    registry.emplace<MouseInput>(camera);
  }
  if (!registry.all_of<Movement>(camera)) {
    registry.emplace<Movement>(camera);
  }
  if (!registry.all_of<ViewportTag>(camera)) {
    registry.emplace<ViewportTag>(camera, ViewportTag{"Render Window"});
  }
  
  LOG_INFO("Active camera changed to entity: {}", static_cast<uint32_t>(camera));
}

entt::entity CameraManagerSystem::createCamera(Scene* scene, const std::string& name) {
  if (!scene) {
    LOG_ERROR("Scene is null in createCamera");
    return entt::null;
  }

  auto& registry = scene->getEntityRegistry();
  auto entity = registry.create();

  auto& transform = registry.emplace<Transform>(entity);
  transform.translation = math::Vector3f{0.0f, 0.0f, 0.0f};
  transform.rotation = math::Vector3f{0.0f, 0.0f, 0.0f};
  transform.scale = math::Vector3f{1.0f, 1.0f, 1.0f};

  // default values
  auto& camera = registry.emplace<Camera>(entity);
  camera.type = CameraType::Perspective;
  camera.fov = math::g_degreeToRadian(90.0f);
  camera.nearClip = 0.1f;
  camera.farClip = 1000.0f;
  camera.width = 1920.0f;
  camera.height = 1080.0f;

  registry.emplace<InputActions>(entity);
  registry.emplace<MouseInput>(entity);
  registry.emplace<Movement>(entity);
  registry.emplace<ViewportTag>(entity, ViewportTag{"Render Window"});

  std::string cameraName = name.empty() ? generateCameraName_(registry) : name;
  LOG_INFO("Created camera: {} (entity: {})", cameraName, static_cast<uint32_t>(entity));

  return entity;
}

bool CameraManagerSystem::deleteCamera(Scene* scene, entt::entity camera) {
  if (!scene) {
    LOG_ERROR("Scene is null in deleteCamera");
    return false;
  }

  auto& registry = scene->getEntityRegistry();
  
  if (!registry.valid(camera) || !registry.all_of<Camera>(camera)) {
    LOG_ERROR("Invalid camera entity provided to deleteCamera");
    return false;
  }

  auto allCameras = getAllCameras(registry);
  if (allCameras.size() <= 1) {
    LOG_WARN("Cannot delete the last camera");
    return false;
  }

  bool wasActive = registry.all_of<ActiveCamera>(camera);

  registry.destroy(camera);
  LOG_INFO("Deleted camera entity: {}", static_cast<uint32_t>(camera));

  if (wasActive) {
    ensureActiveCameraExists(scene);
  }

  return true;
}

void CameraManagerSystem::ensureActiveCameraExists(Scene* scene) {
  if (!scene) {
    LOG_ERROR("Scene is null in ensureActiveCameraExists");
    return;
  }

  auto& registry = scene->getEntityRegistry();
  
  // Check if we already have an active camera
  auto activeView = registry.view<ActiveCamera>();
  if (activeView.begin() != activeView.end()) {
    return;
  }

  // Find any camera and make it active
  auto allCameras = getAllCameras(registry);
  if (!allCameras.empty()) {
    setActiveCamera(registry, allCameras[0]);
    return;
  }

  auto newCamera = createCamera(scene, "Default Camera");
  if (newCamera != entt::null) {
    setActiveCamera(registry, newCamera);
    LOG_INFO("Created default camera as no cameras existed");
  }
}

std::string CameraManagerSystem::generateCameraName_(Registry& registry) {
  auto cameras = getAllCameras(registry);
  return "Camera " + std::to_string(cameras.size() + 1);
}

}  // namespace ecs
}  // namespace arise