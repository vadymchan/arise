#include "config/runtime_settings.h"

#include "utils/logger/log.h"
#include "utils/math/math_util.h"

namespace arise {

using gfx::rhi::RenderingApi;

RuntimeSettings& RuntimeSettings::s_get() {
  static RuntimeSettings instance;
  return instance;
}

RuntimeSettings::RuntimeSettings() {
  auto configManager = ServiceLocator::s_get<ConfigManager>();
  auto configPath    = PathManager::s_getEngineSettingsPath() / "settings.json";
  auto config        = configManager->getConfig(configPath);
  if (!config) {
    configManager->addConfig(configPath);
    config = configManager->getConfig(configPath);
  }
  config->registerConverter<math::Vector3f>(&math::g_getVectorfromConfig);
  updateFromConfig();
}

void RuntimeSettings::updateFromConfig() {
  auto configManager = ServiceLocator::s_get<ConfigManager>();
  auto config        = configManager->getConfig(PathManager::s_getEngineSettingsPath() / "settings.json");

  if (config) {
    m_worldUp_ = config->get<math::Vector3f>("worldUp");
  } else {
    LOG_ERROR(std::string("Failed to initialize RuntimeSettings at - config is nullptr."));
  }
}

const math::Vector3f& RuntimeSettings::getWorldUp() const {
  return m_worldUp_;
}

}  // namespace arise
