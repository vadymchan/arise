#include "ecs/systems/bounding_volume_system.h"

#include "ecs/components/bounding_volume.h"
#include "ecs/components/model.h"
#include "ecs/components/transform.h"
#include "utils/logger/log.h"

namespace arise {
namespace ecs {

void BoundingVolumeSystem::update(Scene* scene, float deltaTime) {
  if (!scene) {
    return;
  }

  auto& registry = scene->getEntityRegistry();

  auto modelsWithoutBounds = registry.view<Transform, Model*>(entt::exclude<WorldBounds>);
  for (auto entity : modelsWithoutBounds) {
    auto& model = registry.get<Model*>(entity);
    if (model) {
      WorldBounds worldBounds;
      worldBounds.boundingBox = bounds::createInvalid();
      worldBounds.isDirty     = true;
      registry.emplace<WorldBounds>(entity, worldBounds);

      LOG_DEBUG("Added WorldBounds component to entity " + std::to_string(static_cast<uint32_t>(entity)));
    }
  }

  auto view = registry.view<Transform, Model*, WorldBounds>();
  for (auto entity : view) {
    updateEntityWorldBounds_(entity, scene);
  }
}

void BoundingVolumeSystem::updateEntityWorldBounds_(entt::entity entity, Scene* scene) {
  auto& registry = scene->getEntityRegistry();

  const auto& transform   = registry.get<Transform>(entity);
  const auto* model       = registry.get<Model*>(entity);
  auto&       worldBounds = registry.get<WorldBounds>(entity);

  bool needsUpdate = worldBounds.isDirty || transform.isDirty;
  if (!needsUpdate) {
    return;
  }

  if (!model) {
    worldBounds.boundingBox = bounds::createInvalid();
    worldBounds.isDirty     = false;
    GlobalLogger::Log(
        LogLevel::Warning,
        "BoundingVolumeSystem: Model* is null for entity " + std::to_string(static_cast<uint32_t>(entity)));
    return;
  }

  const BoundingBox& localBounds = model->boundingBox;

  if (!bounds::isValid(localBounds)) {
    worldBounds.boundingBox = bounds::createInvalid();
    worldBounds.isDirty     = false;
    GlobalLogger::Log(
        LogLevel::Debug,
        "BoundingVolumeSystem: Invalid model bounds for entity " + std::to_string(static_cast<uint32_t>(entity)));
    return;
  }

  math::Matrix4f<> transformMatrix = calculateTransformMatrix(transform);
  worldBounds.boundingBox          = bounds::transformAABB(localBounds, transformMatrix);
  worldBounds.isDirty              = false;

  LOG_DEBUG("BoundingVolumeSystem: Updated world bounds for entity " + std::to_string(static_cast<uint32_t>(entity))
            + " using model: " + model->filePath.string());
}

}  // namespace ecs
}  // namespace arise