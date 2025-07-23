#ifndef ARISE_INPUT_ACTIONS_H
#define ARISE_INPUT_ACTIONS_H

namespace arise {

enum class InputAction {
  MoveForward,
  MoveBackward,
  MoveLeft,
  MoveRight,
  MoveUp,
  MoveDown,
  Count
};

enum class EditorAction {
  SaveScene,
  FocusInspector,
  ShowControls,
  GizmoTranslate,
  GizmoRotate,
  GizmoScale,
  GizmoToggleSpace,
  GizmoToggleVisibility,
  MousePick,
  Count
};

}  // namespace arise

#endif  // ARISE_INPUT_ACTIONS_H