#ifndef ARISE_CAMERA_MANAGER_SYSTEM_H
#define ARISE_CAMERA_MANAGER_SYSTEM_H

#include "scene/scene.h"

#include <entt/entt.hpp>
#include <string>
#include <vector>

namespace arise {
namespace ecs {

class CameraManagerSystem {
  public:
  static entt::entity findActiveCamera(Registry& registry);
  static std::vector<entt::entity> getAllCameras(Registry& registry);
  static void setActiveCamera(Registry& registry, entt::entity camera);
  static entt::entity createCamera(Scene* scene, const std::string& name = "");
  static bool deleteCamera(Scene* scene, entt::entity camera);
  static void ensureActiveCameraExists(Scene* scene);

  private:
  static std::string generateCameraName_(Registry& registry);
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_CAMERA_MANAGER_SYSTEM_H