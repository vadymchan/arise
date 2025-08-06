#ifndef ARISE_SYSTEM_MANAGER_H
#define ARISE_SYSTEM_MANAGER_H

#include "ecs/systems/i_updatable_system.h"
#include "utils/logger/log.h"

namespace arise {
namespace ecs {

// TODO: In future, when other than UpdatableSystems will be introduced,
// consider to separate SystemManager to SubSystems like UpdatableSystemManager,
// ReactiveSystemManager, EventSystemManager, LifecycleSystemManager etc.

/**
 * @class SystemManager
 * @brief Manages all systems within the engine.
 *
 * @details
 * - User should avoid adding duplicate systems.
 * - Systems should not overlap in functionality to prevent unintended behavior.
 */
class SystemManager {
  public:
  void addSystem(std::unique_ptr<IUpdatableSystem> system);

  // TODO: add removeSystem method

  template <typename T>
  T* getSystem() const {
    for (const auto& system : m_systems_) {
      T* typedSystem = dynamic_cast<T*>(system.get());
      if (typedSystem) {
        return typedSystem;
      }
    }
    return nullptr;
  }

  template <typename T, typename... Args>
  bool replaceSystem(Args&&... args) {
    for (auto it = m_systems_.begin(); it != m_systems_.end(); ++it) {
      if (dynamic_cast<T*>(it->get())) {
        m_systems_.erase(it);

        auto newSystem = std::make_unique<T>(std::forward<Args>(args)...);
        addSystem(std::move(newSystem));
        return true;
      }
    }

    LOG_WARN("No existing system found to replace: " + std::string(typeid(T).name()));
    return false;
  }

  void updateSystems(Scene* scene, float deltaTime);

  private:
  std::vector<std::unique_ptr<IUpdatableSystem>> m_systems_;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_SYSTEM_MANAGER_H
