#include "input/editor_input_processor.h"

#include <imgui.h>

namespace arise {

bool EditorInputProcessor::process(const SDL_Event& e) {
  if (!shouldProcess() || e.type != SDL_KEYDOWN) {
    return false;
  }

  auto actionOpt = m_inputMap->getEditorAction(e.key);
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

bool EditorInputProcessor::shouldProcess() const {
  return !ImGui::GetIO().WantCaptureKeyboard;
}

}  // namespace arise