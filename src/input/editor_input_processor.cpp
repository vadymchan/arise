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
  }

  return false;
}

bool EditorInputProcessor::handleKeyboard(const SDL_Event& event) {
  auto actionOpt = m_inputMap->getEditorAction(event.key);
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
  if (event.type == SDL_KEYDOWN) {
    return !ImGui::GetIO().WantCaptureKeyboard;
  }

  if (event.type == SDL_MOUSEBUTTONDOWN) {
    bool wantCapture = ImGui::GetIO().WantCaptureMouse;
    return !wantCapture;
  }

  return false;
}

}  // namespace arise