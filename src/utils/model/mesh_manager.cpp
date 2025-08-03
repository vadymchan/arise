#include "utils/model/mesh_manager.h"

#include "utils/logger/global_logger.h"

namespace arise {

ecs::Mesh* MeshManager::addMesh(std::unique_ptr<ecs::Mesh> mesh, const std::filesystem::path& sourcePath) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!mesh) {
    GlobalLogger::Log(LogLevel::Error, "Attempting to add null mesh");
    return nullptr;
  }

  std::string key = createKey(sourcePath, mesh->meshName);

  auto it = m_meshes.find(key);
  if (it != m_meshes.end()) {
    GlobalLogger::Log(LogLevel::Warning,
                      "Mesh '" + mesh->meshName + "' from " + sourcePath.string() + " already exists. Overwriting.");
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

  GlobalLogger::Log(LogLevel::Warning, "Mesh '" + meshName + "' from " + sourcePath.string() + " not found");
  return nullptr;
}

std::string MeshManager::createKey(const std::filesystem::path& path, const std::string& name) const {
  return path.string() + "#" + name;
}

}  // namespace arise