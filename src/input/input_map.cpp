#include "input/input_map.h"

#include <algorithm>

namespace arise {

const std::unordered_map<std::string, InputAction> InputMap::s_stringToInputAction = {
  { "MoveForward",  InputAction::MoveForward},
  {"MoveBackward", InputAction::MoveBackward},
  {    "MoveLeft",     InputAction::MoveLeft},
  {   "MoveRight",    InputAction::MoveRight},
  {      "MoveUp",       InputAction::MoveUp},
  {    "MoveDown",     InputAction::MoveDown},
  {      "ZoomIn",       InputAction::ZoomIn},
  {     "ZoomOut",      InputAction::ZoomOut},
  {   "ZoomReset",    InputAction::ZoomReset}
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
  {            "MousePick",             EditorAction::MousePick}  // TODO: not completely implemented yet
};

const std::unordered_map<std::string, SDL_Scancode> InputMap::s_keyNameToScancode = {
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
  loadMapFromFile(m_standaloneMapPath, m_standaloneBindings, ApplicationMode::Standalone);
}

void InputMap::loadEditorMap() {
  loadMapFromFile(m_editorMapPath, m_editorBindings, ApplicationMode::Editor);
}

std::optional<InputAction> InputMap::getStandaloneAction(const SDL_KeyboardEvent& event) const {
  for (const auto& [actionName, binding] : m_standaloneBindings) {
    if (binding.matchesEvent(event)) {
      return stringToInputAction(actionName);
    }
  }
  return std::nullopt;
}

std::optional<EditorAction> InputMap::getEditorAction(const SDL_KeyboardEvent& event) const {
  for (const auto& [actionName, binding] : m_editorBindings) {
    if (binding.matchesEvent(event)) {
      return stringToEditorAction(actionName);
    }
  }
  return std::nullopt;
}

void InputMap::loadMapFromFile(const std::filesystem::path&                 path,
                               std::unordered_map<std::string, KeyBinding>& bindings,
                               ApplicationMode                              mode) {
  bindings.clear();

  auto configManager = ServiceLocator::s_get<ConfigManager>();
  if (!configManager) {
    GlobalLogger::Log(LogLevel::Error, "ConfigManager not available");
    return;
  }

  if (!configManager->getConfig(path)) {
    configManager->addConfig(path);
  }

  auto config = configManager->getConfig(path);
  if (!config) {
    std::string modeStr = (mode == ApplicationMode::Editor) ? "editor" : "standalone";
    GlobalLogger::Log(LogLevel::Error, "Failed to load " + modeStr + " input map config: " + path.string());
    return;
  }

  std::vector<std::string> expectedKeys;
  if (mode == ApplicationMode::Standalone) {
    expectedKeys = getStandaloneActionKeys();
  } else if (mode == ApplicationMode::Editor) {
    expectedKeys = getEditorActionKeys();
  }

  for (const auto& actionName : expectedKeys) {
    std::string keyString = config->get<std::string>(actionName);
    if (!keyString.empty()) {
      KeyBinding binding = parseKeyBinding(keyString);
      if (binding.key != SDL_SCANCODE_UNKNOWN) {
        bindings[actionName] = binding;
      } else {
        std::string modeStr = (mode == ApplicationMode::Editor) ? "editor" : "standalone";
        GlobalLogger::Log(LogLevel::Warning, "Unknown key binding in " + modeStr + " map: " + keyString);
      }
    }
  }

  std::string modeStr = (mode == ApplicationMode::Editor) ? "editor" : "standalone";
  GlobalLogger::Log(LogLevel::Info, "Loaded " + std::to_string(bindings.size()) + " " + modeStr + " key bindings");
}

KeyBinding InputMap::parseKeyBinding(const std::string& keyString) const {
  KeyBinding binding;
  binding.key = SDL_SCANCODE_UNKNOWN;

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

  auto it = s_keyNameToScancode.find(processedString);
  if (it != s_keyNameToScancode.end()) {
    binding.key = it->second;
  }

  return binding;
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