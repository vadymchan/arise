#ifndef ARISE_GAME_H
#define ARISE_GAME_H

#include "gfx/renderer/renderer.h"

#include <bitset>
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
  enum class Action {
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    Count,
  };

  Application() {}

  virtual ~Application() {}

  void processInput() {}

  void setup();

  void update(float deltaTime);

  void onMouseMove(int32_t xOffset, int32_t yOffset);

  void release();

  private:
  void setupInputHandlers();

  static constexpr float s_speedChangeStep = 0.5f;
  static constexpr float s_minMovingSpeed  = 0.1f;

  float m_movingSpeed_ = 10.0f;
  // TODO: add key bindings for sensitivity change
  float m_mouseSensitivity_ = 0.1f;

  std::bitset<static_cast<size_t>(Action::Count)> m_actionStates_;

  float m_yaw_                       = 0.0f;
  float m_pitch_                     = 0.0f;
  bool  m_isRightMouseButtonPressed_ = false;

  Scene* m_scene_ = nullptr;
};

}  // namespace arise

#endif  // ARISE_GAME_H