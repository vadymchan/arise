#include "utils/model/render_mesh_manager.h"

#include "utils/buffer/buffer_manager.h"
#include "utils/logger/global_logger.h"
#include "utils/material/material_manager.h"
#include "utils/model/render_geometry_mesh_manager.h"
#include "utils/service/service_locator.h"

namespace arise {
RenderMeshManager::~RenderMeshManager() {
  if (!m_renderMeshes.empty()) {
    GlobalLogger::Log(
        LogLevel::Info,
        "RenderMeshManager destroyed, releasing " + std::to_string(m_renderMeshes.size()) + " render meshes");

    for (const auto& [mesh, renderMesh] : m_renderMeshes) {
      GlobalLogger::Log(LogLevel::Info, "Released render mesh for: " + mesh->meshName);
    }
  }
}

RenderMesh* RenderMeshManager::addRenderMesh(RenderGeometryMesh* gpuMesh, Material* material, Mesh* sourceMesh) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!gpuMesh || !sourceMesh) {
    GlobalLogger::Log(LogLevel::Error, "Cannot create render mesh with null GPU mesh or source mesh");
    return nullptr;
  }

  auto it = m_renderMeshes.find(sourceMesh);
  if (it != m_renderMeshes.end()) {
    GlobalLogger::Log(LogLevel::Warning, "Render mesh already exists for this mesh. Overwriting.");
  }

  auto renderMesh      = std::make_unique<RenderMesh>();
  renderMesh->gpuMesh  = gpuMesh;
  renderMesh->material = material;

  RenderMesh* meshPtr = renderMesh.get();

  m_renderMeshes[sourceMesh] = std::move(renderMesh);

  return meshPtr;
}

RenderMesh* RenderMeshManager::getRenderMesh(Mesh* sourceMesh) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_renderMeshes.find(sourceMesh);
  if (it != m_renderMeshes.end()) {
    return it->second.get();
  }

  GlobalLogger::Log(LogLevel::Warning, "No render mesh found for the specified mesh");
  return nullptr;
}

bool RenderMeshManager::removeRenderMesh(RenderMesh* renderMesh) {
  if (!renderMesh) {
    GlobalLogger::Log(LogLevel::Error, "Cannot remove null render mesh");
    return false;
  }

  GlobalLogger::Log(LogLevel::Info, "Removing render mesh");

  auto renderGeometryMeshManager = ServiceLocator::s_get<RenderGeometryMeshManager>();
  auto bufferManager             = ServiceLocator::s_get<BufferManager>();
  auto materialManager           = ServiceLocator::s_get<MaterialManager>();

  if (renderGeometryMeshManager && renderMesh->gpuMesh) {
    renderGeometryMeshManager->removeRenderGeometryMesh(renderMesh->gpuMesh);
  }

  if (materialManager && renderMesh->material) {
    materialManager->removeMaterial(renderMesh->material);
  }

  if (bufferManager && renderMesh->transformMatrixBuffer) {
    bufferManager->removeBuffer(renderMesh->transformMatrixBuffer);
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  Mesh* sourceMesh = nullptr;
  for (auto it = m_renderMeshes.begin(); it != m_renderMeshes.end(); ++it) {
    if (it->second.get() == renderMesh) {
      sourceMesh = it->first;
      break;
    }
  }

  if (sourceMesh) {
    m_renderMeshes.erase(sourceMesh);
    return true;
  }

  GlobalLogger::Log(LogLevel::Warning, "Render mesh not found in manager");
  return false;
}

}  // namespace arise