#ifndef ARISE_EDITOR_INPUT_PROCESSOR_H
#define ARISE_EDITOR_INPUT_PROCESSOR_H

#include "core/application_mode.h"
#include "input/actions.h"
#include "input/input_map.h"
#include "input/input_processor.h"

#include <SDL.h>

#include <functional>
#include <unordered_map>
#include <vector>

namespace arise {

using EditorCallback = std::function<void(EditorAction)>;

class EditorInputProcessor : public InputProcessor {
  public:
  explicit EditorInputProcessor(InputMap* inputMap, std::function<ApplicationMode()> getCurrentMode)
      : m_inputMap(inputMap)
      , m_getCurrentMode(std::move(getCurrentMode)) {}

  void subscribe(EditorAction action, EditorCallback cb) { m_callbacks[action].push_back(std::move(cb)); }

  bool process(const SDL_Event& e) override;
  int  getPriority() const override { return 100; }

  private:
  bool shouldProcess(const SDL_Event& event) const;
  bool handleKeyboard(const SDL_Event& event);
  bool handleMouse(const SDL_Event& event);

  InputMap*                                                     m_inputMap;
  std::function<ApplicationMode()>                              m_getCurrentMode;
  std::unordered_map<EditorAction, std::vector<EditorCallback>> m_callbacks;
};

}  // namespace arise

#endif  // ARISE_EDITOR_INPUT_PROCESSOR_H