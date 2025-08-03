#ifndef ARISE_MESH_MANAGER_H
#define ARISE_MESH_MANAGER_H

#include "ecs/components/mesh.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace arise {

class MeshManager {
  public:
  MeshManager()  = default;
  ~MeshManager() = default;

  /**
   * @param sourcePath Source file path (used for identification)
   */
  ecs::Mesh* addMesh(std::unique_ptr<ecs::Mesh> mesh, const std::filesystem::path& sourcePath);

  ecs::Mesh* getMesh(const std::filesystem::path& sourcePath, const std::string& meshName);

  private:
  // Map of meshes keyed by unique identifier (path + name)
  // TODO: use std::filesystem::path as a key
  std::unordered_map<std::string, std::unique_ptr<ecs::Mesh>> m_meshes;
  mutable std::mutex                                          m_mutex;

  std::string createKey(const std::filesystem::path& path, const std::string& name) const;
};

}  // namespace arise

#endif  // ARISE_MESH_MANAGER_H