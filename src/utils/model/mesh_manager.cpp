#include "utils/model/mesh_manager.h"

#include "utils/logger/log.h"

namespace arise {

ecs::Mesh* MeshManager::addMesh(std::unique_ptr<ecs::Mesh> mesh, const std::filesystem::path& sourcePath) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!mesh) {
    LOG_ERROR("Attempting to add null mesh");
    return nullptr;
  }

  std::string key = createKey(sourcePath, mesh->meshName);

  auto it = m_meshes.find(key);
  if (it != m_meshes.end()) {
    LOG_WARN("Mesh '" + mesh->meshName + "' from " + sourcePath.string() + " already exists. Overwriting.");
  }

  ecs::Mesh* meshPtr = mesh.get();

  m_meshes[key] = std::move(mesh);

  return meshPtr;
}

ecs::Mesh* MeshManager::getMesh(const std::filesystem::path& sourcePath, const std::string& meshName) {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::string key = createKey(sourcePath, meshName);
  auto        it  = m_meshes.find(key);

  if (it != m_meshes.end()) {
    return it->second.get();
  }

  LOG_WARN("Mesh '" + meshName + "' from " + sourcePath.string() + " not found");
  return nullptr;
}

std::string MeshManager::createKey(const std::filesystem::path& path, const std::string& name) const {
  return path.string() + "#" + name;
}

}  // namespace arise