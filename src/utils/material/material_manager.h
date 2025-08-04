#ifndef ARISE_MATERIAL_MANAGER_H
#define ARISE_MATERIAL_MANAGER_H

#include "ecs/components/material.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace arise {

class MaterialManager {
  public:
  MaterialManager() = default;
  ~MaterialManager();

  std::vector<ecs::Material*> getMaterials(const std::filesystem::path& filepath);

  bool removeMaterial(ecs::Material* material);

  private:
  std::unordered_map<std::filesystem::path, std::vector<std::unique_ptr<ecs::Material>>> materialCache_;
  std::mutex                                                                             m_mutex_;
};

}  // namespace arise

#endif  // ARISE_MATERIAL_MANAGER_H