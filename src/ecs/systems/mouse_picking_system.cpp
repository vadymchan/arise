#include "ecs/systems/mouse_picking_system.h"

#include "ecs/components/bounding_volume.h"
#include "ecs/components/camera.h"
#include "ecs/components/mesh.h"
#include "ecs/components/model.h"
#include "ecs/components/selected.h"
#include "ecs/components/transform.h"
#include "ecs/components/viewport_tag.h"
#include "scene/scene.h"
#include "utils/logger/global_logger.h"

#include <math_library/graphics.h>

#include <algorithm>
#include <limits>

namespace arise {
namespace ecs {

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
    GlobalLogger::Log(LogLevel::Warning, "No active camera found");
    return entt::null;
  }

  auto& registry = scene->getEntityRegistry();
  auto& matrices = registry.get<CameraMatrices>(cameraEntity);

  int viewportWidth  = m_viewportContext->getViewportWidth();
  int viewportHeight = m_viewportContext->getViewportHeight();

  if (viewportWidth <= 0 || viewportHeight <= 0) {
    GlobalLogger::Log(LogLevel::Warning, "Invalid viewport size");
    return entt::null;
  }

  math::Rayf ray
      = createRayFromScreen_(mouseX, mouseY, viewportWidth, viewportHeight, matrices.view, matrices.projection);

  entt::entity hitEntity = performRaycast_(scene, ray);

  if (hitEntity != entt::null) {
    GlobalLogger::Log(LogLevel::Info, "Picked entity " + std::to_string(static_cast<uint32_t>(hitEntity)));
  } else {
    GlobalLogger::Log(LogLevel::Debug, "No entity picked");
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
  std::vector<AABBCandidate> aabbCandidates = performAABBRaycast_(scene, ray);

  if (aabbCandidates.empty()) {
    GlobalLogger::Log(LogLevel::Debug, "No AABB intersections found");
    return entt::null;
  }

  std::sort(aabbCandidates.begin(), aabbCandidates.end(), [](const AABBCandidate& a, const AABBCandidate& b) {
    return a.distance < b.distance;
  });

  entt::entity hitEntity = performTriangleRaycast_(scene, ray, aabbCandidates);

  return hitEntity;
}

std::vector<AABBCandidate> MousePickingSystem::performAABBRaycast_(Scene* scene, const math::Rayf<>& ray) {
  auto& registry = scene->getEntityRegistry();
  auto  view     = registry.view<WorldBounds>();

  std::vector<AABBCandidate> candidates;

  for (auto entity : view) {
    const auto& worldBounds = view.get<WorldBounds>(entity);

    if (!bounds::isValid(worldBounds.boundingBox)) {
      continue;
    }

    math::Point3f minPoint(worldBounds.boundingBox.min);
    math::Point3f maxPoint(worldBounds.boundingBox.max);

    auto result = math::g_rayAABBintersect(ray, minPoint, maxPoint);

    if (result.hit && result.distance > 0) {
      candidates.push_back({entity, result.distance});
    }
  }

  GlobalLogger::Log(LogLevel::Debug, "Found " + std::to_string(candidates.size()) + " AABB candidates");

  return candidates;
}

entt::entity MousePickingSystem::performTriangleRaycast_(Scene*                            scene,
                                                         const math::Rayf<>&               ray,
                                                         const std::vector<AABBCandidate>& candidates) {
  auto& registry = scene->getEntityRegistry();

  for (const auto& candidate : candidates) {
    if (!registry.all_of<Model*, Transform>(candidate.entity)) {
      continue;
    }

    const auto* model     = registry.get<Model*>(candidate.entity);
    const auto& transform = registry.get<Transform>(candidate.entity);

    if (!model || model->meshes.empty()) {
      continue;
    }

    math::Matrix4f<> entityTransform = calculateTransformMatrix(transform);

    float closestDistance = std::numeric_limits<float>::max();
    bool  anyTriangleHit  = false;

    for (const Mesh* mesh : model->meshes) {
      if (!mesh) {
        continue;
      }

      math::Matrix4f<> meshToWorldTransform = mesh->transformMatrix * entityTransform;

      float meshDistance;
      if (rayIntersectsMesh_(ray, mesh, meshToWorldTransform, meshDistance)) {
        if (meshDistance < closestDistance) {
          closestDistance = meshDistance;
          anyTriangleHit  = true;
        }
      }
    }

    if (anyTriangleHit) {
      GlobalLogger::Log(LogLevel::Info,
                        "Triangle hit on entity " + std::to_string(static_cast<uint32_t>(candidate.entity))
                            + " at distance " + std::to_string(closestDistance));
      return candidate.entity;
    }
  }

  GlobalLogger::Log(LogLevel::Debug, "No triangle intersections found");
  return entt::null;
}

bool MousePickingSystem::rayIntersectsMesh_(const math::Rayf<>&     ray,
                                            const Mesh*             mesh,
                                            const math::Matrix4f<>& meshToWorldTransform,
                                            float&                  outDistance) {
  if (!mesh || mesh->vertices.empty() || mesh->indices.empty()) {
    return false;
  }

  math::Matrix4f<> worldToMeshTransform = meshToWorldTransform.inverse();

  math::Vector4f rayOriginHomogeneous(ray.origin(), 1.0f);
  rayOriginHomogeneous *= worldToMeshTransform;
  // TODO: request to math_library - add conversion from Vector4f to Point3f
  math::Point3f localRayOrigin(rayOriginHomogeneous.x(), rayOriginHomogeneous.y(), rayOriginHomogeneous.z());

  math::Vector4f rayDirectionHomogeneous(ray.direction(), 0.0f);
  rayDirectionHomogeneous *= worldToMeshTransform;
  math::Vector3f localRayDirection(
      rayDirectionHomogeneous.x(), rayDirectionHomogeneous.y(), rayDirectionHomogeneous.z());
  localRayDirection = localRayDirection.normalized();

  math::Rayf localRay(localRayOrigin, localRayDirection);

  float closestDistance = std::numeric_limits<float>::max();
  bool  anyHit          = false;

  for (size_t i = 0; i < mesh->indices.size(); i += 3) {
    if (i + 2 >= mesh->indices.size()) {
      break;
    }

    uint32_t idx0 = mesh->indices[i];
    uint32_t idx1 = mesh->indices[i + 1];
    uint32_t idx2 = mesh->indices[i + 2];

    if (idx0 >= mesh->vertices.size() || idx1 >= mesh->vertices.size() || idx2 >= mesh->vertices.size()) {
      continue;
    }

    const auto& v0Local = mesh->vertices[idx0].position;
    const auto& v1Local = mesh->vertices[idx1].position;
    const auto& v2Local = mesh->vertices[idx2].position;

    math::Point3f v0(v0Local);
    math::Point3f v1(v1Local);
    math::Point3f v2(v2Local);

    auto result = math::g_rayTriangleintersect(localRay, v0, v1, v2);

    if (result.hit && result.distance > 0 && result.distance < closestDistance) {
      closestDistance = result.distance;
      anyHit          = true;
    }
  }

  if (anyHit) {
    // Note: Distance in local space may not be the same as world space distance
    // due to scaling in the transform. For accurate world distance, we could
    // transform the intersection point back to world space and calculate distance.
    // For now, we use the local space distance as an approximation.
    outDistance = closestDistance;
  }

  return anyHit;
}

}  // namespace ecs
}  // namespace arise
