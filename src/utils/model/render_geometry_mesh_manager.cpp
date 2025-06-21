#include "utils/model/render_geometry_mesh_manager.h"

#include "utils/buffer/buffer_manager.h"
#include "utils/logger/global_logger.h"
#include "utils/service/service_locator.h"

namespace arise {
RenderGeometryMeshManager::~RenderGeometryMeshManager() {
  if (!m_renderGeometryMeshes.empty()) {
    GlobalLogger::Log(LogLevel::Info,
                      "RenderGeometryMeshManager destroyed, releasing " + std::to_string(m_renderGeometryMeshes.size())
                          + " geometry meshes");

    for (const auto& [mesh, geometryMesh] : m_renderGeometryMeshes) {
      GlobalLogger::Log(LogLevel::Info, "Released geometry mesh for: " + mesh->meshName);
    }
  }
}

RenderGeometryMesh* RenderGeometryMeshManager::addRenderGeometryMesh(
    std::unique_ptr<RenderGeometryMesh> renderGeometryMesh, Mesh* sourceMesh) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!renderGeometryMesh || !sourceMesh) {
    GlobalLogger::Log(LogLevel::Error, "Attempting to add null render geometry mesh or source mesh");
    return nullptr;
  }

  auto it = m_renderGeometryMeshes.find(sourceMesh);
  if (it != m_renderGeometryMeshes.end()) {
    GlobalLogger::Log(LogLevel::Warning, "Render geometry already exists for this mesh. Overwriting.");
  }

  RenderGeometryMesh* meshPtr = renderGeometryMesh.get();

  m_renderGeometryMeshes[sourceMesh] = std::move(renderGeometryMesh);

  return meshPtr;
}

RenderGeometryMesh* RenderGeometryMeshManager::getRenderGeometryMesh(Mesh* sourceMesh) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_renderGeometryMeshes.find(sourceMesh);
  if (it != m_renderGeometryMeshes.end()) {
    return it->second.get();
  }

  GlobalLogger::Log(LogLevel::Warning, "No render geometry found for the specified mesh");
  return nullptr;
}

bool RenderGeometryMeshManager::removeRenderGeometryMesh(RenderGeometryMesh* gpuMesh) {
  if (!gpuMesh) {
    GlobalLogger::Log(LogLevel::Error, "Cannot remove null render geometry mesh");
    return false;
  }

  GlobalLogger::Log(LogLevel::Info, "Removing render geometry mesh");

  auto bufferManager = ServiceLocator::s_get<BufferManager>();

  if (bufferManager) {
    if (gpuMesh->vertexBuffer) {
      bufferManager->removeBuffer(gpuMesh->vertexBuffer);
    }
    if (gpuMesh->indexBuffer) {
      bufferManager->removeBuffer(gpuMesh->indexBuffer);
    }
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  Mesh* sourceMesh = nullptr;
  for (auto it = m_renderGeometryMeshes.begin(); it != m_renderGeometryMeshes.end(); ++it) {
    if (it->second.get() == gpuMesh) {
      sourceMesh = it->first;
      break;
    }
  }

  if (sourceMesh) {
    m_renderGeometryMeshes.erase(sourceMesh);
    return true;
  }

  GlobalLogger::Log(LogLevel::Warning, "Render geometry mesh not found in manager");
  return false;
}

}  // namespace arise