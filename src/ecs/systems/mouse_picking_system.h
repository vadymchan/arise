#ifndef ARISE_MOUSE_PICKING_SYSTEM_H
#define ARISE_MOUSE_PICKING_SYSTEM_H

#include "ecs/systems/i_updatable_system.h"
#include "input/viewport_context.h"

#include <math_library/graphics.h>

namespace arise {

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

  ViewportContext* m_viewportContext = nullptr;
};

}  // namespace arise

#endif  // ARISE_MOUSE_PICKING_SYSTEM_H
