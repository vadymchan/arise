#ifndef ARISE_I_MATERIAL_LOADER_H
#define ARISE_I_MATERIAL_LOADER_H

#include "ecs/components/material.h"

namespace arise {

class IMaterialLoader {
  public:
  virtual ~IMaterialLoader() = default;
  virtual std::vector<std::unique_ptr<ecs::Material>> loadMaterials(const std::filesystem::path& filepath) = 0;
};

}  // namespace arise

#endif  // ARISE_I_MATERIAL_LOADER_H
