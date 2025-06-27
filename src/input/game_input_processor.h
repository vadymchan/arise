#ifndef ARISE_GAME_INPUT_PROCESSOR_H
#define ARISE_GAME_INPUT_PROCESSOR_H

#include "input/input_map.h"
#include "input/input_processor.h"
#include "input/viewport_context.h"

#include <SDL.h>

namespace arise {

class GameInputProcessor : public InputProcessor {
  public:
  GameInputProcessor(InputMap* inputMap, ViewportContext* viewportContext);

  bool process(const SDL_Event& event) override;
  int  getPriority() const override { return 10; }

  private:
  bool handleKeyboard(const SDL_Event& event);
  bool handleMouse(const SDL_Event& event);
  bool shouldProcessInput(const SDL_Event& event) const;

  InputMap*        m_inputMap;
  ViewportContext* m_viewportContext;
};

}  // namespace arise

#endif  // ARISE_GAME_INPUT_PROCESSOR_H