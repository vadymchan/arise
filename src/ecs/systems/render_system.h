#ifndef ARISE_RENDER_SYSTEM_H
#define ARISE_RENDER_SYSTEM_H

#include "ecs/systems/i_updatable_system.h"

namespace arise {
namespace ecs {

class RenderSystem : public IUpdatableSystem {
  public:
  void update(Scene* scene, float deltaTime) override;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_RENDER_SYSTEM_H