#include "utils/path_manager/path_manager.h"

#include "config/config_manager.h"
#include "utils/logger/log.h"
#include "utils/service/service_locator.h"

namespace arise {

std::filesystem::path PathManager::s_getAssetPath() {
  return s_getPath(s_assetPath);
}

std::filesystem::path PathManager::s_getModelPath() {
  return s_getPath(s_modelPath);
}

std::filesystem::path PathManager::s_getShaderPath() {
  return s_getPath(s_shaderPath);
}

std::filesystem::path PathManager::s_getDebugPath() {
  return s_getPath(s_debugPath);
}

std::filesystem::path PathManager::s_getScenesPath() {
  return s_getPath(s_scenesPath);
}

std::filesystem::path PathManager::s_getEngineSettingsPath() {
  return s_getPath(s_engineSettingsPath);
}

std::filesystem::path PathManager::s_getLogoPath() {
  return s_getPath(s_logoPath);
}

bool PathManager::s_isConfigAvailable() {
  if (!s_config_) {
    auto configManager = ServiceLocator::s_get<ConfigManager>();
    if (!configManager) {
      LOG_WARN(
          "ConfigManager is not provided in ServiceLocator when "
          "using PathManager. Adding it...");
      ServiceLocator::s_provide<ConfigManager>();
      configManager = ServiceLocator::s_get<ConfigManager>();
    }
    configManager->addConfig(std::string(s_configFile));

    s_config_ = configManager->getConfig(std::string(s_configFile));

    if (!s_config_) {
      LOG_ERROR("Failed to load config!");
      return false;
    }
  }

  return true;
}

std::filesystem::path PathManager::s_getPath(std::string_view pathKey) {
  if (!s_isConfigAvailable()) {
    LOG_ERROR("Config is not available.");
    return {};
  }

  if (auto config = s_config_) {
    return std::filesystem::path(config->get<std::string>(std::string(pathKey)));
  }

  return {};
}

}  // namespace arise