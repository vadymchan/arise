#include "utils/model/render_geometry_mesh_manager.h"

#include "utils/buffer/buffer_manager.h"
#include "utils/logger/log.h"
#include "utils/service/service_locator.h"

namespace arise {
RenderGeometryMeshManager::~RenderGeometryMeshManager() {
  if (!m_renderGeometryMeshes.empty()) {
    LOG_INFO("RenderGeometryMeshManager destroyed, releasing " + std::to_string(m_renderGeometryMeshes.size())
             + " geometry meshes");

    for (const auto& [mesh, geometryMesh] : m_renderGeometryMeshes) {
      LOG_INFO("Released geometry mesh for: " + mesh->meshName);
    }
  }
}

ecs::RenderGeometryMesh* RenderGeometryMeshManager::addRenderGeometryMesh(
    std::unique_ptr<ecs::RenderGeometryMesh> renderGeometryMesh, ecs::Mesh* sourceMesh) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!renderGeometryMesh || !sourceMesh) {
    LOG_ERROR("Attempting to add null render geometry mesh or source mesh");
    return nullptr;
  }

  auto it = m_renderGeometryMeshes.find(sourceMesh);
  if (it != m_renderGeometryMeshes.end()) {
    LOG_WARN("Render geometry already exists for this mesh. Overwriting.");
  }

  ecs::RenderGeometryMesh* meshPtr = renderGeometryMesh.get();

  m_renderGeometryMeshes[sourceMesh] = std::move(renderGeometryMesh);

  return meshPtr;
}

ecs::RenderGeometryMesh* RenderGeometryMeshManager::getRenderGeometryMesh(ecs::Mesh* sourceMesh) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_renderGeometryMeshes.find(sourceMesh);
  if (it != m_renderGeometryMeshes.end()) {
    return it->second.get();
  }

  LOG_WARN("No render geometry found for the specified mesh");
  return nullptr;
}

bool RenderGeometryMeshManager::removeRenderGeometryMesh(ecs::RenderGeometryMesh* gpuMesh) {
  if (!gpuMesh) {
    LOG_ERROR("Cannot remove null render geometry mesh");
    return false;
  }

  LOG_INFO("Removing render geometry mesh");

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

  ecs::Mesh* sourceMesh = nullptr;
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

  LOG_WARN("Render geometry mesh not found in manager");
  return false;
}

}  // namespace arise