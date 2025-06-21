#ifndef ARISE_PATH_MANAGER_H
#define ARISE_PATH_MANAGER_H

#include "config/config.h"

#include <filesystem>

namespace arise {

class PathManager {
  public:
  static std::filesystem::path s_getAssetPath();
  static std::filesystem::path s_getModelPath();
  static std::filesystem::path s_getShaderPath();
  static std::filesystem::path s_getDebugPath();
  static std::filesystem::path s_getScenesPath();
  static std::filesystem::path s_getEngineSettingsPath();
  static std::filesystem::path s_getLogoPath();

  private:
  static constexpr std::string_view s_assetPath          = "assetPath";
  static constexpr std::string_view s_modelPath          = "modelPath";
  static constexpr std::string_view s_shaderPath         = "shaderPath";
  static constexpr std::string_view s_debugPath          = "debugPath";
  static constexpr std::string_view s_scenesPath         = "scenesPath";
  static constexpr std::string_view s_engineSettingsPath = "engineSettingsPath";
  static constexpr std::string_view s_logoPath           = "logoPath";

  static constexpr std::string_view s_configFile = "config/resources/paths.json";

  static inline Config* s_config_;

  static bool                  s_isConfigAvailable();
  static std::filesystem::path s_getPath(std::string_view pathKey);
};

}  // namespace arise

#endif  // ARISE_PATH_MANAGER_H