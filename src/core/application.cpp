
#include "core/application.h"

#include <config/config_manager.h>
#include <ecs/component_loaders.h>
#include <ecs/components/light.h>
#include <ecs/components/movement.h>
#include <ecs/components/camera.h>
#include <core/engine.h>
#include <input/input_manager.h>
#include <scene/scene_loader.h>
#include <scene/scene_manager.h>
#include <utils/hot_reload/hot_reload_manager.h>
#include <utils/path_manager/path_manager.h>
#include <utils/service/service_locator.h>

#include <random>

namespace arise {

void Application::setup() {
  GlobalLogger::Log(LogLevel::Info, "Application::setup() started");

  setupInputHandlers();

  Registry registry;

  const std::string sceneName    = "scene";
  auto              sceneManager = ServiceLocator::s_get<SceneManager>();

  auto scene = SceneLoader::loadScene(sceneName, sceneManager);

  if (!scene) {
    GlobalLogger::Log(LogLevel::Error, "Failed to load scene: " + sceneName);
    return;
  }

  sceneManager->switchToScene(sceneName);
  m_scene_ = sceneManager->getCurrentScene();

  GlobalLogger::Log(LogLevel::Info, "Application::setup() completed");
}

void Application::onMouseMove(int32_t xOffset, int32_t yOffset) {
  if (!m_isRightMouseButtonPressed_) {
    return;
  }

  // TODO: promote to a member variable for configuration
  float xOffsetFloat = static_cast<float>(xOffset) * m_mouseSensitivity_;
  float yOffsetFloat = static_cast<float>(yOffset) * m_mouseSensitivity_;

  m_yaw_   += xOffsetFloat;
  m_pitch_ += yOffsetFloat;

  // TODO: remove magic numbers
  m_pitch_ = std::clamp(m_pitch_, -89.0f, 89.0f);

  auto& registry     = m_scene_->getEntityRegistry();
  auto  cameraEntity = registry.view<Camera, Transform>().front();

  if (cameraEntity == entt::null) {
    GlobalLogger::Log(LogLevel::Error, "Camera entity is null.");
    return;
  }

  auto& transform = registry.get<Transform>(cameraEntity);

  transform.rotation.x() = m_pitch_;
  transform.rotation.y() = m_yaw_;
}

void Application::update(float deltaTime) {
  // TODO: make event driven
  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  m_scene_          = sceneManager->getCurrentScene();

  // TODO: move to a separate function
  {
    auto& registry     = m_scene_->getEntityRegistry();
    auto  cameraEntity = registry.view<Movement, CameraMatrices>().front();

    if (cameraEntity == entt::null) {
      GlobalLogger::Log(LogLevel::Error, "Camera entity is null");
      return;
    }

    auto& movement = registry.get<Movement>(cameraEntity);

    // TODO: this is more low level thing (engine side), should be moved
    auto cameraMatrices = registry.get<CameraMatrices>(cameraEntity);
    auto cameraRight    = cameraMatrices.view.getColumn<0>().resizedCopy<3>();
    auto cameraUp       = cameraMatrices.view.getColumn<1>().resizedCopy<3>();
    auto cameraForward  = cameraMatrices.view.getColumn<2>().resizedCopy<3>();

    math::Vector3f resultDirection = math::g_zeroVector<float, 3>();

    if (m_actionStates_.test(static_cast<size_t>(Action::MoveForward))) {
      resultDirection += cameraForward;
    }
    if (m_actionStates_.test(static_cast<size_t>(Action::MoveBackward))) {
      resultDirection -= cameraForward;
    }
    if (m_actionStates_.test(static_cast<size_t>(Action::MoveLeft))) {
      resultDirection -= cameraRight;
    }
    if (m_actionStates_.test(static_cast<size_t>(Action::MoveRight))) {
      resultDirection += cameraRight;
    }
    if (m_actionStates_.test(static_cast<size_t>(Action::MoveUp))) {
      resultDirection += cameraUp;
    }
    if (m_actionStates_.test(static_cast<size_t>(Action::MoveDown))) {
      resultDirection -= cameraUp;
    }

    movement.direction = resultDirection;
    movement.strength  = m_movingSpeed_;
  }
}

void Application::release() {
}

void Application::setupInputHandlers() {
  auto keyboardEventHandler = ServiceLocator::s_get<InputManager>()->getKeyboardHandler();

  // Subscribe to key press events to set action states
  keyboardEventHandler->subscribe({SDL_KEYDOWN, SDL_SCANCODE_W}, [this](const KeyboardEvent& event) {
    m_actionStates_.set(static_cast<size_t>(Action::MoveForward), true);
  });

  keyboardEventHandler->subscribe({SDL_KEYDOWN, SDL_SCANCODE_S}, [this](const KeyboardEvent& event) {
    if ((SDL_GetModState() & KMOD_CTRL) != 0) {  // do not move while hold Ctrl (for Ctrl + S operation)
      return;
    }
    m_actionStates_.set(static_cast<size_t>(Action::MoveBackward), true);
  });

  keyboardEventHandler->subscribe({SDL_KEYDOWN, SDL_SCANCODE_A}, [this](const KeyboardEvent& event) {
    m_actionStates_.set(static_cast<size_t>(Action::MoveLeft), true);
  });

  keyboardEventHandler->subscribe({SDL_KEYDOWN, SDL_SCANCODE_D}, [this](const KeyboardEvent& event) {
    m_actionStates_.set(static_cast<size_t>(Action::MoveRight), true);
  });

  keyboardEventHandler->subscribe({SDL_KEYDOWN, SDL_SCANCODE_E}, [this](const KeyboardEvent& event) {
    m_actionStates_.set(static_cast<size_t>(Action::MoveUp), true);
  });

  keyboardEventHandler->subscribe({SDL_KEYDOWN, SDL_SCANCODE_Q}, [this](const KeyboardEvent& event) {
    m_actionStates_.set(static_cast<size_t>(Action::MoveDown), true);
  });

  // Subscribe to key release events to reset action states
  keyboardEventHandler->subscribe({SDL_KEYUP, SDL_SCANCODE_W}, [this](const KeyboardEvent& event) {
    m_actionStates_.set(static_cast<size_t>(Action::MoveForward), false);
  });

  keyboardEventHandler->subscribe({SDL_KEYUP, SDL_SCANCODE_S}, [this](const KeyboardEvent& event) {
    m_actionStates_.set(static_cast<size_t>(Action::MoveBackward), false);
  });

  keyboardEventHandler->subscribe({SDL_KEYUP, SDL_SCANCODE_A}, [this](const KeyboardEvent& event) {
    m_actionStates_.set(static_cast<size_t>(Action::MoveLeft), false);
  });

  keyboardEventHandler->subscribe({SDL_KEYUP, SDL_SCANCODE_D}, [this](const KeyboardEvent& event) {
    m_actionStates_.set(static_cast<size_t>(Action::MoveRight), false);
  });

  keyboardEventHandler->subscribe({SDL_KEYUP, SDL_SCANCODE_E}, [this](const KeyboardEvent& event) {
    m_actionStates_.set(static_cast<size_t>(Action::MoveUp), false);
  });

  keyboardEventHandler->subscribe({SDL_KEYUP, SDL_SCANCODE_Q}, [this](const KeyboardEvent& event) {
    m_actionStates_.set(static_cast<size_t>(Action::MoveDown), false);
  });

  auto mouseEventHandler = ServiceLocator::s_get<InputManager>()->getMouseHandler();

  mouseEventHandler->subscribe({SDL_MOUSEWHEEL}, [this](const MouseWheelEvent& event) {
    if ((SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT)) == 0) {
      return;
    }

    if (event.y > 0) {
      m_movingSpeed_ += s_speedChangeStep;
    } else if (event.y < 0) {
      m_movingSpeed_ -= s_speedChangeStep;
    }

    m_movingSpeed_ = std::clamp(m_movingSpeed_, s_minMovingSpeed, 100.0f);
  });

  mouseEventHandler->subscribe({SDL_MOUSEMOTION},
                               [this](const MouseMotionEvent& event) { this->onMouseMove(event.xrel, event.yrel); });

  mouseEventHandler->subscribe({SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT}, [this](const MouseButtonEvent& event) {
    auto& registry     = m_scene_->getEntityRegistry();
    auto  cameraEntity = registry.view<Camera, Transform>().front();

    if (cameraEntity != entt::null) {
      auto& transform = registry.get<Transform>(cameraEntity);
      m_pitch_        = transform.rotation.x();
      m_yaw_          = transform.rotation.y();
    }

    m_isRightMouseButtonPressed_ = true;
    SDL_SetRelativeMouseMode(SDL_TRUE);
  });

  mouseEventHandler->subscribe({SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT}, [this](const MouseButtonEvent& event) {
    m_isRightMouseButtonPressed_ = false;
    SDL_SetRelativeMouseMode(SDL_FALSE);
  });
}

}  // namespace arise
