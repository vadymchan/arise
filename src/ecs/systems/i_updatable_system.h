#ifndef ARISE_I_UPDATABLE_SYSTEM_H
#define ARISE_I_UPDATABLE_SYSTEM_H

#include "scene/scene.h"

namespace arise {
namespace ecs {

/**
 * TODO: Future system types can be added here:
 * - IReactiveSystem: For systems that respond to specific events
 * - IEventSystem: For systems that handle specific events or messages
 * - ILifecycleSystem: For systems that manage entity lifecycles
 **/

class IUpdatableSystem {
  public:
  virtual ~IUpdatableSystem() = default;

  virtual void update(Scene* scene, float deltaTime) = 0;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_I_UPDATABLE_SYSTEM_H