#include "ecs/systems/mouse_picking_system.h"

#include "ecs/components/bounding_volume.h"
#include "ecs/components/camera.h"
#include "ecs/components/selected.h"
#include "ecs/components/transform.h"
#include "ecs/components/viewport_tag.h"
#include "scene/scene.h"
#include "utils/logger/global_logger.h"

#include <math_library/graphics.h>

#include <algorithm>
#include <limits>

namespace arise {

void MousePickingSystem::update(Scene* scene, float deltaTime) {
  // This system primarily responds to input events
  // The update method can be used for any per-frame logic if needed
  // For now, it's essentially a no-op since all work is done in handleMousePick
}

entt::entity MousePickingSystem::handleMousePick(Scene* scene, int mouseX, int mouseY) {
  if (!scene || !m_viewportContext) {
    return entt::null;
  }

  return processMouseClick_(scene, mouseX, mouseY);
}

entt::entity MousePickingSystem::processMouseClick_(Scene* scene, int mouseX, int mouseY) {
  entt::entity cameraEntity = findActiveCamera_(scene);
  if (cameraEntity == entt::null) {
    GlobalLogger::Log(LogLevel::Warning, "MousePickingSystem: No active camera found");
    return entt::null;
  }

  auto& registry = scene->getEntityRegistry();
  auto& matrices = registry.get<CameraMatrices>(cameraEntity);

  int viewportWidth  = m_viewportContext->getViewportWidth();
  int viewportHeight = m_viewportContext->getViewportHeight();

  if (viewportWidth <= 0 || viewportHeight <= 0) {
    GlobalLogger::Log(LogLevel::Warning, "MousePickingSystem: Invalid viewport size");
    return entt::null;
  }

  math::Rayf ray
      = createRayFromScreen_(mouseX, mouseY, viewportWidth, viewportHeight, matrices.view, matrices.projection);

  entt::entity hitEntity = performRaycast_(scene, ray);

  if (hitEntity != entt::null) {
    GlobalLogger::Log(LogLevel::Info,
                      "MousePickingSystem: Picked entity " + std::to_string(static_cast<uint32_t>(hitEntity)));
  } else {
    GlobalLogger::Log(LogLevel::Debug, "MousePickingSystem: No entity picked");
  }

  return hitEntity;
}

entt::entity MousePickingSystem::findActiveCamera_(Scene* scene) {
  auto& registry = scene->getEntityRegistry();

  auto view = registry.view<Transform, Camera, CameraMatrices>();

  if (view.begin() != view.end()) {
    return *view.begin();
  }

  return entt::null;
}

math::Rayf<> MousePickingSystem::createRayFromScreen_(int                     mouseX,
                                                      int                     mouseY,
                                                      int                     viewportWidth,
                                                      int                     viewportHeight,
                                                      const math::Matrix4f<>& viewMatrix,
                                                      const math::Matrix4f<>& projMatrix) {
  return math::g_screenToRay<float>(static_cast<float>(mouseX),
                                    static_cast<float>(mouseY),
                                    static_cast<float>(viewportWidth),
                                    static_cast<float>(viewportHeight),
                                    viewMatrix,
                                    projMatrix);
}

entt::entity MousePickingSystem::performRaycast_(Scene* scene, const math::Rayf<>& ray) {
  auto& registry = scene->getEntityRegistry();

  auto view = registry.view<WorldBounds>();

  entt::entity closestEntity   = entt::null;
  float        closestDistance = std::numeric_limits<float>::max();

  for (auto entity : view) {
    const auto& worldBounds = view.get<WorldBounds>(entity);

    if (!bounds::isValid(worldBounds.boundingBox)) {
      continue;
    }

    math::Point3f minPoint(worldBounds.boundingBox.min);
    math::Point3f maxPoint(worldBounds.boundingBox.max);

    auto result = math::g_rayAABBintersect(ray, minPoint, maxPoint);

    if (result.hit && result.distance > 0 && result.distance < closestDistance) {
      closestDistance = result.distance;
      closestEntity   = entity;
    }
  }

  if (closestEntity != entt::null) {
    GlobalLogger::Log(LogLevel::Info,
                      "MousePickingSystem: Hit entity " + std::to_string(static_cast<uint32_t>(closestEntity))
                          + " at distance " + std::to_string(closestDistance));
  }

  return closestEntity;
}

}  // namespace arise
