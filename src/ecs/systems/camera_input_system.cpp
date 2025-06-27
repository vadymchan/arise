#include "ecs/systems/camera_input_system.h"

#include "ecs/components/camera.h"
#include "ecs/components/input_components.h"
#include "ecs/components/movement.h"
#include "ecs/components/transform.h"
#include "utils/logger/global_logger.h"
#include "utils/math/math_util.h"

#include <algorithm>

namespace arise {

void CameraInputSystem::update(Scene* scene, float dt) {
  auto& registry = scene->getEntityRegistry();
  auto  view     = registry.view<InputActions, MouseInput, ViewportTag, Movement, Transform, Camera>();

  if (view.begin() == view.end()) {
    // GlobalLogger::Log(LogLevel::Warning, "CameraInputSystem: No camera entities found with required components");
    return;
  }

  for (auto entity : view) {
    const auto& viewportTag = view.get<ViewportTag>(entity);

    if (m_viewportContext && !m_viewportContext->isViewportFocused(viewportTag.id)) {
      // GlobalLogger::Log(LogLevel::Debug, "CameraInputSystem: Viewport not focused, skipping camera input");
      continue;
    }

    auto& actions   = view.get<InputActions>(entity);
    auto& mouse     = view.get<MouseInput>(entity);
    auto& movement  = view.get<Movement>(entity);
    auto& transform = view.get<Transform>(entity);

    if (mouse.rightButtonPressed) {
      handleMouseLook(mouse, transform);
      handleSpeedChange(mouse, movement);
      handleMovement(actions, movement, entity, registry);
    } else {
      clearCameraInput(movement);
    }
        
    mouse.clearDeltas();
  }
}

void CameraInputSystem::handleMouseLook(MouseInput& mouse, Transform& transform) {
  if (mouse.deltaX != 0.0f || mouse.deltaY != 0.0f) {
    transform.rotation.y() += mouse.deltaX * m_mouseSensitivity;
    transform.rotation.x() += mouse.deltaY * m_mouseSensitivity;
    transform.rotation.x()  = std::clamp(transform.rotation.x(), -89.0f, 89.0f);
    transform.isDirty       = true;
  }
}

void CameraInputSystem::handleSpeedChange(MouseInput& mouse, Movement& movement) {
  if (mouse.wheelDelta != 0.0f) {
    m_currentMouseSpeed += mouse.wheelDelta;
  }
  movement.strength = std::clamp(m_currentMouseSpeed, m_minMovingSpeed, m_maxMovingSpeed);
}

void CameraInputSystem::handleMovement(const InputActions& actions,
                                       Movement&           movement,
                                       entt::entity        entity,
                                       Registry&           registry) {
  if (!registry.all_of<CameraMatrices>(entity)) {
    return;
  }

  auto& matrices = registry.get<CameraMatrices>(entity);
  auto  right    = matrices.view.getColumn<0>().resizedCopy<3>();
  auto  up       = matrices.view.getColumn<1>().resizedCopy<3>();
  auto  forward  = matrices.view.getColumn<2>().resizedCopy<3>();

  math::Vector3f direction = math::g_zeroVector<float, 3>();

  if (actions.isActive(InputAction::MoveForward)) {
    direction += forward;
  }
  if (actions.isActive(InputAction::MoveBackward)) {
    direction -= forward;
  }
  if (actions.isActive(InputAction::MoveLeft)) {
    direction -= right;
  }
  if (actions.isActive(InputAction::MoveRight)) {
    direction += right;
  }
  if (actions.isActive(InputAction::MoveUp)) {
    direction += up;
  }
  if (actions.isActive(InputAction::MoveDown)) {
    direction -= up;
  }

  movement.direction = direction;
}

void CameraInputSystem::clearCameraInput(Movement& movement) {
  movement.direction = math::g_zeroVector<float, 3>();
  movement.strength  = 0.0f;
}

}  // namespace arise