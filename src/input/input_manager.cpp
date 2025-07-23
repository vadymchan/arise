#include "input/input_manager.h"

#include "utils/logger/global_logger.h"

namespace arise {

InputManager::InputManager()
    : m_router(std::make_unique<InputRouter>())
    , m_inputMap(std::make_unique<InputMap>())
    , m_viewportContext(std::make_unique<ViewportContext>()) {
  createDefault();
}

void InputManager::createDefault() {
  auto gameProcessor = std::make_unique<GameInputProcessor>(m_inputMap.get(), m_viewportContext.get());
  m_router->addProcessor(std::move(gameProcessor));
  GlobalLogger::Log(LogLevel::Info, "InputManager: Created default GameInputProcessor");
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

}  // namespace arise