#include "input/editor_input_processor.h"

#include "utils/logger/global_logger.h"

#include <imgui.h>

namespace arise {

bool EditorInputProcessor::process(const SDL_Event& event) {
  if (!shouldProcess(event)) {
    return false;
  }

  switch (event.type) {
    case SDL_KEYDOWN:
      return handleKeyboard(event);
    case SDL_MOUSEBUTTONDOWN:
      return handleMouse(event);
  }

  return false;
}

bool EditorInputProcessor::handleKeyboard(const SDL_Event& event) {
  auto actionOpt = m_inputMap->getEditorKeyboardAction(event.key);
  if (!actionOpt) {
    return false;
  }

  auto it = m_callbacks.find(*actionOpt);
  if (it != m_callbacks.end()) {
    for (auto& callback : it->second) {
      callback(*actionOpt);
    }
  }

  return true;
}

bool EditorInputProcessor::handleMouse(const SDL_Event& event) {
  auto actionOpt = m_inputMap->getEditorMouseAction(event.button);
  if (!actionOpt) {
    return false;
  }

  auto it = m_callbacks.find(*actionOpt);
  if (it != m_callbacks.end()) {
    for (auto& callback : it->second) {
      callback(*actionOpt);
    }
  }

  return true;
}

bool EditorInputProcessor::shouldProcess(const SDL_Event& event) const {
  // Always allow processing toggle mode action regardless of current mode
  if (event.type == SDL_KEYDOWN) {
    auto actionOpt = m_inputMap->getEditorKeyboardAction(event.key);
    if (actionOpt && *actionOpt == EditorAction::ToggleApplicationMode) {
      return true;
    }
  }

  if (m_getCurrentMode() != ApplicationMode::Editor) {
    return false;
  }

  if (event.type == SDL_KEYDOWN) {
    // In Editor mode, check if ImGui wants to capture keyboard
    return !ImGui::GetIO().WantCaptureKeyboard;
  }

  if (event.type == SDL_MOUSEBUTTONDOWN) {
    bool wantCapture = ImGui::GetIO().WantCaptureMouse;
    return !wantCapture;
  }

  return false;
}

}  // namespace arise