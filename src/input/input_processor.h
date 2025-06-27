#ifndef ARISE_INPUT_PROCESSOR_H
#define ARISE_INPUT_PROCESSOR_H

#include <SDL.h>

namespace arise {

class InputProcessor {
  public:
  virtual ~InputProcessor() = default;

  virtual bool process(const SDL_Event& event) = 0;
  virtual int  getPriority() const             = 0;
};

}  // namespace arise

#endif  // ARISE_INPUT_PROCESSOR_H
