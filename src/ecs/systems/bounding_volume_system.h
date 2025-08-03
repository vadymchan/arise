#ifndef ARISE_BOUNDING_VOLUME_SYSTEM_H
#define ARISE_BOUNDING_VOLUME_SYSTEM_H

#include "ecs/systems/i_updatable_system.h"

namespace arise {
namespace ecs {

/**
 * System for managing and updating bounding volumes of entities
 * Works only with CPU-side Model* data, follows SoC principle
 */
class BoundingVolumeSystem : public IUpdatableSystem {
  public:
  BoundingVolumeSystem()  = default;
  ~BoundingVolumeSystem() = default;

  void update(Scene* scene, float deltaTime) override;

  private:
  void updateEntityWorldBounds_(entt::entity entity, Scene* scene);
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_BOUNDING_VOLUME_SYSTEM_H