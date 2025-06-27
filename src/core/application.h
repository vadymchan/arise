#ifndef ARISE_GAME_H
#define ARISE_GAME_H

#include "gfx/renderer/renderer.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iterator>
#include <vector>

namespace arise {

class Engine;

class Application {
  public:
  Application() {}

  virtual ~Application() {}

  void processInput() {}

  void setup();

  void update(float deltaTime);

  void release();

  private:
  Scene* m_scene_ = nullptr;
};

}  // namespace arise

#endif  // ARISE_GAME_H