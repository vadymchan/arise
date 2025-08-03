#ifndef ARISE_CAMERA_SYSTEM_H
#define ARISE_CAMERA_SYSTEM_H

#include "ecs/systems/i_updatable_system.h"

namespace arise {
namespace ecs {

class CameraSystem : public IUpdatableSystem {
  public:
  void update(Scene* scene, float deltaTime) override;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_CAMERA_SYSTEM_H
