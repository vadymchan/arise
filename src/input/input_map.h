#ifndef ARISE_INPUT_MAP_H
#define ARISE_INPUT_MAP_H

#include "config/config_manager.h"
#include "core/application_mode.h"
#include "input/actions.h"
#include "utils/logger/global_logger.h"
#include "utils/path_manager/path_manager.h"
#include "utils/service/service_locator.h"

#include <SDL.h>

#include <filesystem>
#include <optional>
#include <unordered_map>

namespace arise {

struct KeyBinding {
  SDL_Scancode key           = SDL_SCANCODE_UNKNOWN;
  bool         requiresCtrl  = false;
  bool         requiresShift = false;
  bool         requiresAlt   = false;

  bool matchesEvent(const SDL_KeyboardEvent& event) const {
    if (event.keysym.scancode != key) {
      return false;
    }

    Uint16 modState     = SDL_GetModState();
    bool   ctrlPressed  = (modState & KMOD_CTRL) != 0;
    bool   shiftPressed = (modState & KMOD_SHIFT) != 0;
    bool   altPressed   = (modState & KMOD_ALT) != 0;

    return (requiresCtrl == ctrlPressed) && (requiresShift == shiftPressed) && (requiresAlt == altPressed);
  }
};

class InputMap {
  public:
  InputMap();

  void loadAllMaps();
  void loadStandaloneMap();
  void loadEditorMap();

  std::optional<InputAction>  getStandaloneAction(const SDL_KeyboardEvent& event) const;
  std::optional<EditorAction> getEditorAction(const SDL_KeyboardEvent& event) const;

  private:
  void loadMapFromFile(const std::filesystem::path&                 path,
                       std::unordered_map<std::string, KeyBinding>& bindings,
                       ApplicationMode                              mode);

  KeyBinding parseKeyBinding(const std::string& keyString) const;

  std::vector<std::string> getStandaloneActionKeys() const;
  std::vector<std::string> getEditorActionKeys() const;

  std::optional<InputAction>  stringToInputAction(const std::string& actionName) const;
  std::optional<EditorAction> stringToEditorAction(const std::string& actionName) const;

  std::unordered_map<std::string, KeyBinding> m_standaloneBindings;
  std::unordered_map<std::string, KeyBinding> m_editorBindings;
  std::filesystem::path                       m_standaloneMapPath;
  std::filesystem::path                       m_editorMapPath;

  static const std::unordered_map<std::string, InputAction>  s_stringToInputAction;
  static const std::unordered_map<std::string, EditorAction> s_stringToEditorAction;
  static const std::unordered_map<std::string, SDL_Scancode> s_keyNameToScancode;
};

}  // namespace arise

#endif  // ARISE_INPUT_MAP_H