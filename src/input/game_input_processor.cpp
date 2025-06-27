#include "input/game_input_processor.h"

#include "ecs/components/camera.h"
#include "ecs/components/input_components.h"
#include "ecs/components/movement.h"
#include "ecs/components/transform.h"
#include "ecs/components/viewport_tag.h"
#include "scene/scene_manager.h"
#include "utils/service/service_locator.h"

#include <SDL.h>

namespace arise {

GameInputProcessor::GameInputProcessor(InputMap* inputMap, ViewportContext* viewportContext)
    : m_inputMap(inputMap)
    , m_viewportContext(viewportContext) {
}

bool GameInputProcessor::process(const SDL_Event& event) {
  if (!shouldProcessInput(event)) {
    return false;
  }

  switch (event.type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      return handleKeyboard(event);
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEWHEEL:
      return handleMouse(event);
  }

  return false;
}

bool GameInputProcessor::handleKeyboard(const SDL_Event& event) {
  auto scene = ServiceLocator::s_get<SceneManager>()->getCurrentScene();
  if (!scene) {
    return false;
  }

  auto& registry   = scene->getEntityRegistry();
  auto  cameraView = registry.view<Camera, InputActions, ViewportTag>();

  if (cameraView.begin() == cameraView.end()) {
    return false;
  }

  for (auto entity : cameraView) {
    const auto& viewportTag = cameraView.get<ViewportTag>(entity);
    if (m_viewportContext && !m_viewportContext->isViewportFocused(viewportTag.id)) {
      continue;
    }
    auto& actions = cameraView.get<InputActions>(entity);

    switch (event.type) {
      case SDL_KEYDOWN:
        if (event.key.repeat == 0) {
          auto action = m_inputMap->getStandaloneAction(event.key);
          if (action.has_value()) {
            actions.setActive(action.value(), true);
          }
        }
        break;
      case SDL_KEYUP:
        auto action = m_inputMap->getStandaloneAction(event.key);
        if (action.has_value()) {
          actions.setActive(action.value(), false);
        }
        break;
    }
  }

  return true;
}

bool GameInputProcessor::handleMouse(const SDL_Event& event) {
  auto scene = ServiceLocator::s_get<SceneManager>()->getCurrentScene();
  if (!scene) {
    return false;
  }

  auto& registry   = scene->getEntityRegistry();
  auto  cameraView = registry.view<Camera, MouseInput, ViewportTag>();

  if (cameraView.begin() == cameraView.end()) {
    return false;
  }

  for (auto entity : cameraView) {
    const auto& viewportTag = cameraView.get<ViewportTag>(entity);
    if (m_viewportContext && !m_viewportContext->isViewportFocused(viewportTag.id)) {
      continue;
    }
    auto& mouse = cameraView.get<MouseInput>(entity);

    switch (event.type) {
      case SDL_MOUSEMOTION:
        if (m_viewportContext) {
          m_viewportContext->updateMousePosition(event.motion.x, event.motion.y);
        }
        mouse.deltaX = event.motion.xrel;
        mouse.deltaY = event.motion.yrel;
        break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        if (event.button.button == SDL_BUTTON_RIGHT) {
          mouse.rightButtonPressed = (event.type == SDL_MOUSEBUTTONDOWN);
          SDL_SetRelativeMouseMode(mouse.rightButtonPressed ? SDL_TRUE : SDL_FALSE);
        }
        break;
      case SDL_MOUSEWHEEL:
        mouse.wheelDelta = event.wheel.y;
        break;
    }
  }

  return true;
}

bool GameInputProcessor::shouldProcessInput(const SDL_Event& e) const {
  // standalone mode: process all events - we don't care about viewport focus
  if (!m_viewportContext) {
    return true;
  }

  // process release button event even if we are not in viewport
  if (e.type == SDL_MOUSEBUTTONUP) {
    return true;
  }

  // process the events even when we ouside viewport
  if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
    return true;
  }

  bool focused = m_viewportContext->isViewportFocused("Render Window");

  if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
    return focused;
  }

  int mx = m_viewportContext->getMouseX();
  int my = m_viewportContext->getMouseY();
  int vx = m_viewportContext->getViewportX();
  int vy = m_viewportContext->getViewportY();
  int vw = m_viewportContext->getViewportWidth();
  int vh = m_viewportContext->getViewportHeight();

  bool mouseInViewport = (mx >= vx && mx < vx + vw && my >= vy && my < vy + vh);
  return focused && mouseInViewport;
}

}  // namespace arise