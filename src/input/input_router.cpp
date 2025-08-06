#include "input/input_router.h"

#include "utils/logger/log.h"

#include <algorithm>

namespace arise {

void InputRouter::route(const SDL_Event& event) {
  for (auto& processor : m_processors) {
    if (processor->process(event)) {
      break;
    }
  }
}

void InputRouter::addProcessor(std::unique_ptr<InputProcessor> processor) {
  m_processors.push_back(std::move(processor));
  sortProcessors();
}

void InputRouter::clear() {
  m_processors.clear();
}

void InputRouter::sortProcessors() {
  std::sort(m_processors.begin(),
            m_processors.end(),
            [](const std::unique_ptr<InputProcessor>& a, const std::unique_ptr<InputProcessor>& b) {
              return a->getPriority() > b->getPriority();
            });
}

}  // namespace arise
