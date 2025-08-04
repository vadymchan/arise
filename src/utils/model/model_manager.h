#ifndef ARISE_MODEL_MANAGER_H
#define ARISE_MODEL_MANAGER_H

#include "utils/model/model_loader_manager.h"
#include "utils/service/service_locator.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace arise {

class ModelManager {
  public:
  ModelManager() = default;

  ecs::Model* getModel(const std::filesystem::path& filepath);

  private:
  std::unordered_map<std::filesystem::path, std::unique_ptr<ecs::Model>> modelCache_;
  std::mutex                                                             mutex_;
};

}  // namespace arise

#endif  // ARISE_MODEL_MANAGER_H
