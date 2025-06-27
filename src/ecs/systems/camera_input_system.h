#ifndef ARISE_CAMERA_INPUT_SYSTEM_H
#define ARISE_CAMERA_INPUT_SYSTEM_H

#include "ecs/components/input_components.h"
#include "ecs/components/movement.h"
#include "ecs/components/transform.h"
#include "ecs/components/viewport_tag.h"
#include "ecs/systems/i_updatable_system.h"
#include "input/viewport_context.h"

#include <entt/entt.hpp>

namespace arise {

class CameraInputSystem : public IUpdatableSystem {
  public:
  explicit CameraInputSystem(ViewportContext* viewportContext = nullptr)
      : m_viewportContext(viewportContext) {}

  void update(Scene* scene, float dt) override;

  void setMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
  void setMinMaxSpeed(float min, float max) {
    m_minMovingSpeed = min;
    m_maxMovingSpeed = max;
  }

  private:
  void handleMovement(const InputActions& actions, Movement& movement, entt::entity entity, Registry& registry);
  void handleMouseLook(MouseInput& mouse, Transform& transform);
  void handleSpeedChange(MouseInput& mouse, Movement& movement);
  void clearCameraInput(Movement& movement);

  ViewportContext* m_viewportContext;
  float            m_mouseSensitivity  = 0.1f;
  float            m_currentMouseSpeed = 5.0f;
  float            m_minMovingSpeed    = 0.1f;
  float            m_maxMovingSpeed    = 100.0f;
};

}  // namespace arise

#endif  // ARISE_CAMERA_INPUT_SYSTEM_H