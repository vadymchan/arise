#ifndef ARISE_I_MODEL_LOADER_H
#define ARISE_I_MODEL_LOADER_H

#include "ecs/components/model.h"

#include <filesystem>
#include <memory>

namespace arise {

class IModelLoader {
  public:
  virtual ~IModelLoader()                                                              = default;
  virtual std::unique_ptr<ecs::Model> loadModel(const std::filesystem::path& filepath) = 0;
};

}  // namespace arise

#endif  // ARISE_I_MODEL_LOADER_H
