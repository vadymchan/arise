#ifndef ARISE_INPUT_MANAGER
#define ARISE_INPUT_MANAGER

#include "input/game_input_processor.h"
#include "input/input_map.h"
#include "input/input_processor.h"
#include "input/input_router.h"
#include "input/viewport_context.h"

#include <SDL.h>

#include <memory>

namespace arise {

class InputManager {
  public:
  InputManager();

  void routeEvent(const SDL_Event& event);
  void addProcessor(std::unique_ptr<InputProcessor> processor) { m_router->addProcessor(std::move(processor)); }

  InputRouter*     getRouter() const noexcept { return m_router.get(); }
  ViewportContext& getViewportContext() { return *m_viewportContext; }
  InputMap*        getInputMap() const { return m_inputMap.get(); }

  void updateViewport(int32_t width,
                      int32_t height,
                      int32_t viewportX      = 0,
                      int32_t viewportY      = 0,
                      int32_t viewportWidth  = 0,
                      int32_t viewportHeight = 0);

  private:
  void createDefault();

  std::unique_ptr<InputRouter>     m_router;
  std::unique_ptr<InputMap>        m_inputMap;
  std::unique_ptr<ViewportContext> m_viewportContext;
};

}  // namespace arise

#endif  // ARISE_INPUT_MANAGER