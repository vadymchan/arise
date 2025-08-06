#include "scene/scene.h"

#include "utils/logger/log.h"
#include "utils/model/render_model_manager.h"
#include "utils/service/service_locator.h"

namespace arise {

Scene::Scene(Registry registry)
    : entityRegistry_(std::move(registry)) {
}

Registry& Scene::getEntityRegistry() {
  return entityRegistry_;
}

const Registry& Scene::getEntityRegistry() const {
  return entityRegistry_;
}

void Scene::setEntityRegistry(Registry registry) {
  entityRegistry_ = std::move(registry);
}

}  // namespace arise
