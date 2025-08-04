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
  ZoomIn,
  ZoomOut,
  ZoomReset,
  ToggleApplicationMode,
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
  ToggleApplicationMode,
  Count
};

}  // namespace arise

#endif  // ARISE_INPUT_ACTIONS_H