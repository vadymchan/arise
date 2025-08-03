#ifndef ARISE_MOUSE_PICKING_SYSTEM_H
#define ARISE_MOUSE_PICKING_SYSTEM_H

#include "ecs/systems/i_updatable_system.h"
#include "input/viewport_context.h"

#include <math_library/graphics.h>
#include <vector>

namespace arise {
namespace ecs {

struct Mesh;

struct AABBCandidate {
  entt::entity entity;
  float        distance;
};

class MousePickingSystem : public IUpdatableSystem {
  public:
  explicit MousePickingSystem(ViewportContext* viewportContext = nullptr)
      : m_viewportContext(viewportContext) {}

  void update(Scene* scene, float deltaTime) override;

  void setViewportContext(ViewportContext* viewportContext) { m_viewportContext = viewportContext; }

  entt::entity handleMousePick(Scene* scene, int mouseX, int mouseY);

  private:
  entt::entity processMouseClick_(Scene* scene, int mouseX, int mouseY);

  entt::entity findActiveCamera_(Scene* scene);

  math::Rayf<> createRayFromScreen_(int                     mouseX,
                                    int                     mouseY,
                                    int                     viewportWidth,
                                    int                     viewportHeight,
                                    const math::Matrix4f<>& viewMatrix,
                                    const math::Matrix4f<>& projMatrix);

  entt::entity performRaycast_(Scene* scene, const math::Rayf<>& ray);

  std::vector<AABBCandidate> performAABBRaycast_(Scene* scene, const math::Rayf<>& ray);
  
  entt::entity performTriangleRaycast_(Scene* scene, const math::Rayf<>& ray, const std::vector<AABBCandidate>& candidates);
  
  bool rayIntersectsMesh_(const math::Rayf<>& ray, 
                         const Mesh* mesh, 
                         const math::Matrix4f<>& meshToWorldTransform,
                         float& outDistance);

  ViewportContext* m_viewportContext = nullptr;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_MOUSE_PICKING_SYSTEM_H
