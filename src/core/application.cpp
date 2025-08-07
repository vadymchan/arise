
#include "core/application.h"

#include <config/config_manager.h>
#include <core/engine.h>
#include <ecs/component_loaders.h>
#include <ecs/components/camera.h>
#include <ecs/components/light.h>
#include <ecs/components/movement.h>
#include <input/input_manager.h>
#include <scene/scene_loader.h>
#include <scene/scene_manager.h>
#include <utils/hot_reload/hot_reload_manager.h>
#include <utils/path_manager/path_manager.h>
#include <utils/service/service_locator.h>

#include <random>

namespace arise {

void Application::setup() {
  LOG_INFO("Application::setup() started");

  Registry registry;

  const std::string sceneName    = "scene";
  auto              sceneManager = ServiceLocator::s_get<SceneManager>();

  auto scene = SceneLoader::loadScene(sceneName, sceneManager);

  if (!scene) {
    LOG_ERROR("Failed to load scene: {}", sceneName);
    return;
  }

  sceneManager->switchToScene(sceneName);
  m_scene_ = sceneManager->getCurrentScene();

  LOG_INFO("Application::setup() completed");
}

void Application::update(float deltaTime) {
  // TODO: make event driven
  auto sceneManager = ServiceLocator::s_get<SceneManager>();
  m_scene_          = sceneManager->getCurrentScene();
}

void Application::release() {
}

}  // namespace arise
