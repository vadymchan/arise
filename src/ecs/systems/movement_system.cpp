#include "ecs/systems/movement_system.h"

#include "ecs/components/movement.h"
#include "ecs/components/transform.h"

namespace arise {
namespace ecs {

void MovementSystem::update(Scene* scene, float deltaTime) {
  Registry& registry = scene->getEntityRegistry();
  auto      view     = registry.view<Transform, Movement>();

  for (auto entity : view) {
    auto& transform = view.get<Transform>(entity);
    auto& movement  = view.get<Movement>(entity);

    transform.translation += movement.direction * movement.strength * deltaTime;

    movement = Movement{};  // Reset movement
  }
}

}  // namespace ecs
}  // namespace arise