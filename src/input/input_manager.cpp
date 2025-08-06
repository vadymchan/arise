#include "input/input_manager.h"

#include "utils/logger/log.h"

namespace arise {

InputManager::InputManager()
    : m_router(std::make_unique<InputRouter>())
    , m_inputMap(std::make_unique<InputMap>())
    , m_viewportContext(std::make_unique<ViewportContext>()) {
  createDefault();
}

void InputManager::createDefault() {
  m_getCurrentMode   = []() { return ApplicationMode::Standalone; };
  auto gameProcessor = std::make_unique<GameInputProcessor>(m_inputMap.get(), m_viewportContext.get(), this);
  m_router->addProcessor(std::move(gameProcessor));
  LOG_INFO("InputManager: Created default GameInputProcessor");
}

void InputManager::updateViewport(int32_t width,
                                  int32_t height,
                                  int32_t viewportX,
                                  int32_t viewportY,
                                  int32_t viewportWidth,
                                  int32_t viewportHeight) {
  if (viewportWidth == 0) {
    viewportWidth = width;
  }
  if (viewportHeight == 0) {
    viewportHeight = height;
  }

  m_viewportContext->updateViewport(viewportX, viewportY, viewportWidth, viewportHeight);
}

void InputManager::routeEvent(const SDL_Event& event) {
  m_router->route(event);
}

void InputManager::setApplicationModeCallback(std::function<ApplicationMode()> callback) {
  m_getCurrentMode = std::move(callback);
  LOG_INFO("InputManager: Application mode callback updated");
}

ApplicationMode InputManager::getCurrentApplicationMode() const {
  if (m_getCurrentMode) {
    return m_getCurrentMode();
  }
  return ApplicationMode::Standalone;  // Default fallback
}

}  // namespace arise