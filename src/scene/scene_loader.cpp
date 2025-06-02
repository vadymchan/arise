
#include "scene/scene_loader.h"

#include "config/config_manager.h"
#include "ecs/component_loaders.h"
#include "ecs/components/camera.h"
#include "ecs/components/light.h"
#include "ecs/components/movement.h"
#include "ecs/components/transform.h"
#include "utils/logger/global_logger.h"
#include "utils/model/render_model_manager.h"
#include "utils/path_manager/path_manager.h"
#include "utils/service/service_locator.h"

#include <algorithm>

namespace arise {

Scene* SceneLoader::loadScene(const std::string& sceneName, SceneManager* sceneManager) {
  auto configPath = PathManager::s_getScenesPath() / (sceneName + ".json");
  return loadSceneFromFile(configPath, sceneManager, sceneName);
}

Scene* SceneLoader::loadSceneFromFile(const std::filesystem::path& configPath,
                                      SceneManager*                sceneManager,
                                      const std::string&           customSceneName) {
  auto configManager = ServiceLocator::s_get<ConfigManager>();
  auto config        = configManager->getConfig(configPath);

  if (!config) {
    configManager->addConfig(configPath);
    config = configManager->getConfig(configPath);
    if (!config) {
      GlobalLogger::Log(LogLevel::Error, "Failed to load scene config: " + configPath.string());
      return nullptr;
    }
    config->registerConverter<Transform>(&g_loadTransform);
    config->registerConverter<Camera>(&g_loadCamera);
    config->registerConverter<Light>(&g_loadLight);
    config->registerConverter<DirectionalLight>(&g_loadDirectionalLight);
    config->registerConverter<PointLight>(&g_loadPointLight);
    config->registerConverter<SpotLight>(&g_loadSpotLight);
  }

  auto                jsonStr = config->toString();
  rapidjson::Document document;
  document.Parse(jsonStr.c_str());

  if (document.HasParseError()) {
    GlobalLogger::Log(LogLevel::Error, "Failed to parse scene JSON: " + configPath.string());
    return nullptr;
  }

  std::string sceneName = customSceneName;
  if (sceneName.empty() && document.HasMember("name") && document["name"].IsString()) {
    sceneName = document["name"].GetString();
  }

  if (sceneName.empty()) {
    GlobalLogger::Log(LogLevel::Error, "Scene name is missing in config: " + configPath.string());
    return nullptr;
  }

  sceneManager->addScene(sceneName, Registry());
  auto  scene    = sceneManager->getScene(sceneName);
  auto& registry = scene->getEntityRegistry();

  if (document.HasMember("entities") && document["entities"].IsArray()) {
    for (auto& entityJson : document["entities"].GetArray()) {
      g_createEntityFromConfig(registry, entityJson);
    }
  }

  return scene;
}

}  // namespace arise