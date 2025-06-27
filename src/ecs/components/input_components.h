#ifndef ARISE_INPUT_COMPONENTS_H
#define ARISE_INPUT_COMPONENTS_H

#include "input/actions.h"

#include <bitset>
#include <string>

namespace arise {

struct InputActions {
  std::bitset<static_cast<size_t>(InputAction::Count)> actions;

  bool isActive(InputAction action) const { return actions.test(static_cast<size_t>(action)); }

  void setActive(InputAction action, bool active) { actions.set(static_cast<size_t>(action), active); }
};

struct MouseInput {
  float deltaX             = 0.0f;
  float deltaY             = 0.0f;
  float wheelDelta         = 0.0f;
  bool  rightButtonPressed = false;
  void  clearDeltas() { deltaX = deltaY = wheelDelta = 0.0f; }
};

}  // namespace arise

#endif  // ARISE_INPUT_COMPONENTS_H