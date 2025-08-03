#ifndef ARISE_CAMERA_INPUT_SYSTEM_H
#define ARISE_CAMERA_INPUT_SYSTEM_H

#include "ecs/components/camera.h"
#include "ecs/components/input_components.h"
#include "ecs/components/movement.h"
#include "ecs/components/transform.h"
#include "ecs/components/viewport_tag.h"
#include "ecs/systems/i_updatable_system.h"
#include "input/viewport_context.h"

#include <math_library/graphics.h>

#include <entt/entt.hpp>

namespace arise {
namespace ecs {

class CameraInputSystem : public IUpdatableSystem {
  public:
  explicit CameraInputSystem(ViewportContext* viewportContext = nullptr)
      : m_viewportContext(viewportContext) {}

  void update(Scene* scene, float dt) override;

  private:
  void handleMovement(const InputActions& actions, Movement& movement, entt::entity entity, Registry& registry);
  void handleMouseLook(MouseInput& mouse, Transform& transform);
  void handleSpeedChange(MouseInput& mouse, Movement& movement);
  void handleZoom(const InputActions& actions, Camera& camera, float deltaTime);
  void clearCameraInput(Movement& movement);

  ViewportContext* m_viewportContext;
  float            m_currentMouseSpeed = 5.0f;
  float            m_mouseSensitivity  = 0.1f; 

  static constexpr float s_kZoomSpeed    = math::g_degreeToRadian(30.0f); 
  static constexpr float s_kMinFov       = math::g_degreeToRadian(10.0f); 
  static constexpr float s_kMaxFov       = math::g_degreeToRadian(120.0f);
  static constexpr float s_kDefaultFov   = math::g_degreeToRadian(90.0f); 
  static constexpr float s_kMinMovingSpeed = 0.1f;
  static constexpr float s_kMaxMovingSpeed = 100.0f;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_CAMERA_INPUT_SYSTEM_H