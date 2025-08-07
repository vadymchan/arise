#include "input/input_map.h"

#include <algorithm>

namespace arise {

const std::unordered_map<std::string, InputAction> InputMap::s_stringToInputAction = {
  {          "MoveForward",           InputAction::MoveForward},
  {         "MoveBackward",          InputAction::MoveBackward},
  {             "MoveLeft",              InputAction::MoveLeft},
  {            "MoveRight",             InputAction::MoveRight},
  {               "MoveUp",                InputAction::MoveUp},
  {             "MoveDown",              InputAction::MoveDown},
  {               "ZoomIn",                InputAction::ZoomIn},
  {              "ZoomOut",               InputAction::ZoomOut},
  {            "ZoomReset",             InputAction::ZoomReset},
  {"ToggleApplicationMode", InputAction::ToggleApplicationMode}
};

const std::unordered_map<std::string, EditorAction> InputMap::s_stringToEditorAction = {
  {            "SaveScene",             EditorAction::SaveScene},
  {       "FocusInspector",        EditorAction::FocusInspector},
  {         "ShowControls",          EditorAction::ShowControls},
  {       "GizmoTranslate",        EditorAction::GizmoTranslate},
  {          "GizmoRotate",           EditorAction::GizmoRotate},
  {           "GizmoScale",            EditorAction::GizmoScale},
  {     "GizmoToggleSpace",      EditorAction::GizmoToggleSpace},
  {"GizmoToggleVisibility", EditorAction::GizmoToggleVisibility},
  {            "MousePick",             EditorAction::MousePick},
  {"ToggleApplicationMode", EditorAction::ToggleApplicationMode}
};

const std::unordered_map<std::string, MouseButton> InputMap::s_mouseNameToButton = {
  {  "LeftMouse",   SDL_BUTTON_LEFT},
  { "RightMouse",  SDL_BUTTON_RIGHT},
  {"MiddleMouse", SDL_BUTTON_MIDDLE}
};

const std::unordered_map<std::string, PhysicalKey> InputMap::s_keyNameToPhysicalKey = {
  {         "A",         SDL_SCANCODE_A},
  {         "B",         SDL_SCANCODE_B},
  {         "C",         SDL_SCANCODE_C},
  {         "D",         SDL_SCANCODE_D},
  {         "E",         SDL_SCANCODE_E},
  {         "F",         SDL_SCANCODE_F},
  {         "G",         SDL_SCANCODE_G},
  {         "H",         SDL_SCANCODE_H},
  {         "I",         SDL_SCANCODE_I},
  {         "J",         SDL_SCANCODE_J},
  {         "K",         SDL_SCANCODE_K},
  {         "L",         SDL_SCANCODE_L},
  {         "M",         SDL_SCANCODE_M},
  {         "N",         SDL_SCANCODE_N},
  {         "O",         SDL_SCANCODE_O},
  {         "P",         SDL_SCANCODE_P},
  {         "Q",         SDL_SCANCODE_Q},
  {         "R",         SDL_SCANCODE_R},
  {         "S",         SDL_SCANCODE_S},
  {         "T",         SDL_SCANCODE_T},
  {         "U",         SDL_SCANCODE_U},
  {         "V",         SDL_SCANCODE_V},
  {         "W",         SDL_SCANCODE_W},
  {         "X",         SDL_SCANCODE_X},
  {         "Y",         SDL_SCANCODE_Y},
  {         "Z",         SDL_SCANCODE_Z},

  {         "1",         SDL_SCANCODE_1},
  {         "2",         SDL_SCANCODE_2},
  {         "3",         SDL_SCANCODE_3},
  {         "4",         SDL_SCANCODE_4},
  {         "5",         SDL_SCANCODE_5},
  {         "6",         SDL_SCANCODE_6},
  {         "7",         SDL_SCANCODE_7},
  {         "8",         SDL_SCANCODE_8},
  {         "9",         SDL_SCANCODE_9},
  {         "0",         SDL_SCANCODE_0},

  {        "F1",        SDL_SCANCODE_F1},
  {        "F2",        SDL_SCANCODE_F2},
  {        "F3",        SDL_SCANCODE_F3},
  {        "F4",        SDL_SCANCODE_F4},
  {        "F5",        SDL_SCANCODE_F5},
  {        "F6",        SDL_SCANCODE_F6},
  {        "F7",        SDL_SCANCODE_F7},
  {        "F8",        SDL_SCANCODE_F8},
  {        "F9",        SDL_SCANCODE_F9},
  {       "F10",       SDL_SCANCODE_F10},
  {       "F11",       SDL_SCANCODE_F11},
  {       "F12",       SDL_SCANCODE_F12},

  {     "Space",     SDL_SCANCODE_SPACE},
  {     "Enter",    SDL_SCANCODE_RETURN},
  {    "Escape",    SDL_SCANCODE_ESCAPE},
  {       "Tab",       SDL_SCANCODE_TAB},
  { "Backspace", SDL_SCANCODE_BACKSPACE},
  {    "Delete",    SDL_SCANCODE_DELETE},
  {    "Insert",    SDL_SCANCODE_INSERT},
  {      "Home",      SDL_SCANCODE_HOME},
  {       "End",       SDL_SCANCODE_END},
  {    "PageUp",    SDL_SCANCODE_PAGEUP},
  {  "PageDown",  SDL_SCANCODE_PAGEDOWN},

  {      "Left",      SDL_SCANCODE_LEFT},
  {     "Right",     SDL_SCANCODE_RIGHT},
  {        "Up",        SDL_SCANCODE_UP},
  {      "Down",      SDL_SCANCODE_DOWN},

  { "LeftShift",    SDL_SCANCODE_LSHIFT},
  {"RightShift",    SDL_SCANCODE_RSHIFT},
  {  "LeftCtrl",     SDL_SCANCODE_LCTRL},
  { "RightCtrl",     SDL_SCANCODE_RCTRL},
  {   "LeftAlt",      SDL_SCANCODE_LALT},
  {  "RightAlt",      SDL_SCANCODE_RALT}
};

InputMap::InputMap() {
  auto pathManager    = ServiceLocator::s_get<PathManager>();
  auto inputPath      = pathManager->s_getAssetPath() / "input";
  m_standaloneMapPath = inputPath / "standalone_map.json";
  m_editorMapPath     = inputPath / "editor_map.json";
  loadAllMaps();
}

void InputMap::loadAllMaps() {
  loadStandaloneMap();
  loadEditorMap();
}

void InputMap::loadStandaloneMap() {
  std::unordered_map<std::string, MouseBinding> emptyMouseBindings;
  loadMapFromFile(m_standaloneMapPath, m_standaloneBindings, emptyMouseBindings, ApplicationMode::Standalone);
}

void InputMap::loadEditorMap() {
  loadMapFromFile(m_editorMapPath, m_editorBindings, m_editorMouseBindings, ApplicationMode::Editor);
}

std::optional<InputAction> InputMap::getStandaloneAction(const SDL_KeyboardEvent& event) const {
  for (const auto& [actionName, binding] : m_standaloneBindings) {
    if (binding.matchesEvent(event)) {
      return stringToInputAction(actionName);
    }
  }
  return std::nullopt;
}

std::optional<EditorAction> InputMap::getEditorKeyboardAction(const SDL_KeyboardEvent& event) const {
  for (const auto& [actionName, binding] : m_editorBindings) {
    if (binding.matchesEvent(event)) {
      return stringToEditorAction(actionName);
    }
  }
  return std::nullopt;
}

std::optional<EditorAction> InputMap::getEditorMouseAction(const SDL_MouseButtonEvent& event) const {
  for (const auto& [actionName, binding] : m_editorMouseBindings) {
    if (binding.matchesEvent(event)) {
      return stringToEditorAction(actionName);
    }
  }
  return std::nullopt;
}

void InputMap::loadMapFromFile(const std::filesystem::path&                   path,
                               std::unordered_map<std::string, KeyBinding>&   keyBindings,
                               std::unordered_map<std::string, MouseBinding>& mouseBindings,
                               ApplicationMode                                mode) {
  keyBindings.clear();
  mouseBindings.clear();

  auto configManager = ServiceLocator::s_get<ConfigManager>();
  if (!configManager) {
    LOG_ERROR("ConfigManager not available");
    return;
  }

  if (!configManager->getConfig(path)) {
    configManager->addConfig(path);
  }

  auto config = configManager->getConfig(path);
  if (!config) {
    std::string modeStr = (mode == ApplicationMode::Editor) ? "editor" : "standalone";
    LOG_ERROR("Failed to load {} input map config: {}", modeStr, path.string());
    return;
  }

  std::vector<std::string> expectedKeys;
  if (mode == ApplicationMode::Standalone) {
    expectedKeys = getStandaloneActionKeys();
  } else if (mode == ApplicationMode::Editor) {
    expectedKeys = getEditorActionKeys();
  }

  std::string modeStr = (mode == ApplicationMode::Editor) ? "editor" : "standalone";

  for (const auto& actionName : expectedKeys) {
    std::string bindingString = config->get<std::string>(actionName);
    if (!bindingString.empty()) {
      processBinding(actionName, bindingString, keyBindings, mouseBindings, modeStr);
    }
  }

  LOG_INFO(
      "Loaded {} key bindings and {} mouse bindings for {} mode", keyBindings.size(), mouseBindings.size(), modeStr);
}

KeyBinding InputMap::parseKeyBinding(const std::string& keyString) const {
  KeyBinding binding;
  binding.key = static_cast<PhysicalKey>(SDL_SCANCODE_UNKNOWN);

  std::string processedString = keyString;

  std::string ctrlStr = "Ctrl+";
  size_t      ctrlPos = processedString.find(ctrlStr);
  if (ctrlPos == 0) {
    binding.requiresCtrl = true;
    processedString      = processedString.substr(ctrlStr.size());
  }

  std::string shiftStr = "Shift+";
  size_t      shiftPos = processedString.find(shiftStr);
  if (shiftPos == 0) {
    binding.requiresShift = true;
    processedString       = processedString.substr(shiftStr.size());
  }

  std::string altStr = "Alt+";
  size_t      altPos = processedString.find(altStr);
  if (altPos == 0) {
    binding.requiresAlt = true;
    processedString     = processedString.substr(altStr.size());
  }

  auto it = s_keyNameToPhysicalKey.find(processedString);
  if (it != s_keyNameToPhysicalKey.end()) {
    binding.key = it->second;
  }

  return binding;
}

MouseBinding InputMap::parseMouseBinding(const std::string& mouseString) const {
  MouseBinding binding;
  binding.button = 0;

  auto it = s_mouseNameToButton.find(mouseString);
  if (it != s_mouseNameToButton.end()) {
    binding.button = it->second;
  }

  return binding;
}

bool InputMap::isMouseBinding(const std::string& bindingString) const {
  return s_mouseNameToButton.find(bindingString) != s_mouseNameToButton.end();
}

void InputMap::processBinding(const std::string&                             actionName,
                              const std::string&                             bindingString,
                              std::unordered_map<std::string, KeyBinding>&   keyBindings,
                              std::unordered_map<std::string, MouseBinding>& mouseBindings,
                              const std::string&                             modeStr) const {
  if (isMouseBinding(bindingString)) {
    processMouseBinding(actionName, bindingString, mouseBindings, modeStr);
  } else {
    processKeyBinding(actionName, bindingString, keyBindings, modeStr);
  }
}

void InputMap::processKeyBinding(const std::string&                           actionName,
                                 const std::string&                           bindingString,
                                 std::unordered_map<std::string, KeyBinding>& keyBindings,
                                 const std::string&                           modeStr) const {
  KeyBinding binding = parseKeyBinding(bindingString);
  if (binding.key != static_cast<PhysicalKey>(SDL_SCANCODE_UNKNOWN)) {
    keyBindings[actionName] = binding;
  } else {
    LOG_WARN("Unknown key binding in {} map: {}", modeStr, bindingString);
  }
}

void InputMap::processMouseBinding(const std::string&                             actionName,
                                   const std::string&                             bindingString,
                                   std::unordered_map<std::string, MouseBinding>& mouseBindings,
                                   const std::string&                             modeStr) const {
  MouseBinding binding = parseMouseBinding(bindingString);
  if (binding.button != 0) {
    mouseBindings[actionName] = binding;
  } else {
    LOG_WARN("Unknown mouse binding in {} map: {}", modeStr, bindingString);
  }
}

std::vector<std::string> InputMap::getStandaloneActionKeys() const {
  std::vector<std::string> keys;
  for (const auto& [actionName, _] : s_stringToInputAction) {
    keys.push_back(actionName);
  }
  return keys;
}

std::vector<std::string> InputMap::getEditorActionKeys() const {
  std::vector<std::string> keys;
  for (const auto& [actionName, _] : s_stringToEditorAction) {
    keys.push_back(actionName);
  }
  return keys;
}

std::optional<InputAction> InputMap::stringToInputAction(const std::string& actionName) const {
  auto it = s_stringToInputAction.find(actionName);
  if (it != s_stringToInputAction.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<EditorAction> InputMap::stringToEditorAction(const std::string& actionName) const {
  auto it = s_stringToEditorAction.find(actionName);
  if (it != s_stringToEditorAction.end()) {
    return it->second;
  }
  return std::nullopt;
}

}  // namespace arise