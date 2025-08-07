
#include "scene/scene_loader.h"

#include "config/config_manager.h"
#include "ecs/component_loaders.h"
#include "ecs/components/camera.h"
#include "ecs/components/light.h"
#include "ecs/components/movement.h"
#include "ecs/components/transform.h"
#include "utils/logger/log.h"
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
      LOG_ERROR("Failed to load scene config: {}", configPath.string());
      return nullptr;
    }
    config->registerConverter<ecs::Transform>(&ecs::g_loadTransform);
    config->registerConverter<ecs::Camera>(&ecs::g_loadCamera);
    config->registerConverter<ecs::Light>(&ecs::g_loadLight);
    config->registerConverter<ecs::DirectionalLight>(&ecs::g_loadDirectionalLight);
    config->registerConverter<ecs::PointLight>(&ecs::g_loadPointLight);
    config->registerConverter<ecs::SpotLight>(&ecs::g_loadSpotLight);
  }

  auto                jsonStr = config->toString();
  rapidjson::Document document;
  document.Parse(jsonStr.c_str());

  if (document.HasParseError()) {
    LOG_ERROR("Failed to parse scene JSON: {}", configPath.string());
    return nullptr;
  }

  std::string sceneName = customSceneName;
  if (sceneName.empty() && document.HasMember("name") && document["name"].IsString()) {
    sceneName = document["name"].GetString();
  }

  if (sceneName.empty()) {
    LOG_ERROR("Scene name is missing in config: {}", configPath.string());
    return nullptr;
  }

  sceneManager->addScene(sceneName, Registry());
  auto  scene    = sceneManager->getScene(sceneName);
  auto& registry = scene->getEntityRegistry();

  if (document.HasMember("entities") && document["entities"].IsArray()) {
    for (auto& entityJson : document["entities"].GetArray()) {
      ecs::g_createEntityFromConfig(registry, entityJson);
    }
  }

  return scene;
}

}  // namespace arise