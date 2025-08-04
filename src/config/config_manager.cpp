#include "config/config_manager.h"

#include "file_loader/file_system_manager.h"
#include "utils/logger/global_logger.h"

#include <ranges>

namespace arise {

void ConfigManager::loadAllConfigsFromDirectory(const std::filesystem::path& dirPath) {
  if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
    GlobalLogger::Log(LogLevel::Error, "Failed to load configs: Directory does not exist: " + dirPath.string());
    return;
  }

  for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
    if (entry.is_regular_file() && entry.path().extension() == std::string(s_configExtension)) {
      addConfig(entry.path());
    }
  }
}

void ConfigManager::addConfig(const std::filesystem::path& filePath) {
  if (configs_.contains(filePath)) {
    GlobalLogger::Log(LogLevel::Warning, "Config already loaded: " + filePath.string());
    return;
  }

  auto config = std::make_unique<Config>();
  if (config->loadFromFile(filePath)) {
    configs_[filePath] = std::move(config);
    GlobalLogger::Log(LogLevel::Info, "Config loaded: " + filePath.string());
  } else {
    GlobalLogger::Log(LogLevel::Error, "Failed to load config: " + filePath.string());
  }
}

Config* ConfigManager::getConfig(const std::filesystem::path& filePath) {
  auto it = configs_.find(filePath);
  if (it != configs_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void ConfigManager::saveAllConfigs() {
  for (const auto& config : configs_ | std::views::values) {
    FileSystemManager::writeFile(config->getFilename(), config->toString());
  }
}

bool ConfigManager::unloadConfig(const std::filesystem::path& filePath) {
  if (configs_.erase(filePath)) {
    GlobalLogger::Log(LogLevel::Info, "Config " + filePath.string() + " unloaded from memory.");
    return true;
  } else {
    GlobalLogger::Log(LogLevel::Error, "Config " + filePath.string() + " not found.");
    return false;
  }
}

void ConfigManager::unloadAllConfigs() {
  configs_.clear();
  GlobalLogger::Log(LogLevel::Info, "All configs have been unloaded from memory.");
}

}  // namespace arise
