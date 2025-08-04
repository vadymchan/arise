#include "ecs/systems/camera_system.h"

#include "config/runtime_settings.h"
#include "ecs/components/camera.h"
#include "ecs/components/transform.h"

#include <math_library/graphics.h>
#include <math_library/quaternion.h>

namespace arise {
namespace ecs {

void CameraSystem::update(Scene* scene, float deltaTime) {
  Registry&             registry        = scene->getEntityRegistry();
  auto&                 runtimeSettings = RuntimeSettings::s_get();
  const math::Vector3f& worldUp         = runtimeSettings.getWorldUp();
  auto                  view            = registry.view<Transform, Camera>();

  for (auto entity : view) {
    if (!registry.any_of<CameraMatrices>(entity)) {
      registry.emplace<CameraMatrices>(entity);
    }

    auto& transform = registry.get<Transform>(entity);
    auto& camera    = registry.get<Camera>(entity);
    auto& matrices  = registry.get<CameraMatrices>(entity);

    // 1: View matrix
    auto pitch = transform.rotation.x();
    auto yaw   = transform.rotation.y();
    auto roll  = transform.rotation.z();
    pitch      = math::g_degreeToRadian(pitch);
    yaw        = math::g_degreeToRadian(yaw);
    roll       = math::g_degreeToRadian(roll);
    // auto q     = math::Quaternionf::fromEulerAngles(roll, pitch, yaw, math::EulerRotationOrder::ZXY);
    auto q = math::Quaternionf::fromEulerAngles(pitch, yaw, roll, math::EulerRotationOrder::XYZ);
    // Assume that the forward vector is (0, 0, 1)
    auto forward   = math::g_forwardVector<float, 3>();
    auto direction = q.rotateVector(forward);
    matrices.view  = math::g_lookToLh(transform.translation, direction, worldUp);

    // 2: Projection matrix
    if (camera.type == CameraType::Perspective) {
      float aspectRatio   = camera.width / camera.height;
      matrices.projection = math::g_perspectiveLhZo(camera.fov, aspectRatio, camera.nearClip, camera.farClip);
    } else if (camera.type == CameraType::Orthographic) {
      matrices.projection = math::g_orthoLhZo(camera.width, camera.height, camera.nearClip, camera.farClip);
    }
  }
}
}  // namespace ecs
}  // namespace arise