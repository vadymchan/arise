#ifndef ARISE_INPUT_ROUTER_H
#define ARISE_INPUT_ROUTER_H

#include "input/input_processor.h"

#include <SDL.h>

#include <memory>
#include <vector>

namespace arise {

class InputRouter {
  public:
  InputRouter()  = default;
  ~InputRouter() = default;

  void route(const SDL_Event& event);
  void addProcessor(std::unique_ptr<InputProcessor> processor);
  void clear();

  private:
  void sortProcessors();

  std::vector<std::unique_ptr<InputProcessor>> m_processors;
};

}  // namespace arise

#endif  // ARISE_INPUT_ROUTER_H
