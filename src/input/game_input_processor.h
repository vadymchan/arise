#ifndef ARISE_GAME_INPUT_PROCESSOR_H
#define ARISE_GAME_INPUT_PROCESSOR_H

#include "core/application_mode.h"
#include "input/input_map.h"
#include "input/input_processor.h"
#include "input/viewport_context.h"

#include <SDL.h>

#include <functional>

namespace arise {

class InputManager;

class GameInputProcessor : public InputProcessor {
  public:
  GameInputProcessor(InputMap* inputMap, ViewportContext* viewportContext, InputManager* inputManager);

  bool process(const SDL_Event& event) override;
  int  getPriority() const override { return 10; }

  private:
  bool handleKeyboard(const SDL_Event& event);
  bool handleMouse(const SDL_Event& event);
  bool shouldProcessInput(const SDL_Event& event) const;

  InputMap*        m_inputMap;
  ViewportContext* m_viewportContext;
  InputManager*    m_inputManager;
};

}  // namespace arise

#endif  // ARISE_GAME_INPUT_PROCESSOR_H