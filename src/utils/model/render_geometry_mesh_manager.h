#ifndef ARISE_RENDER_GEOMETRY_MESH_MANAGER_H
#define ARISE_RENDER_GEOMETRY_MESH_MANAGER_H

#include "ecs/components/mesh.h"
#include "ecs/components/render_geometry_mesh.h"

#include <memory>
#include <mutex>
#include <unordered_map>

namespace arise {

class RenderGeometryMeshManager {
  public:
  RenderGeometryMeshManager() = default;
  ~RenderGeometryMeshManager();

  /**
   * @param sourceMesh Associated CPU mesh for identification
   */
  ecs::RenderGeometryMesh* addRenderGeometryMesh(std::unique_ptr<ecs::RenderGeometryMesh> renderGeometryMesh,
                                                 ecs::Mesh*                               sourceMesh);

  ecs::RenderGeometryMesh* getRenderGeometryMesh(ecs::Mesh* sourceMesh);

  bool removeRenderGeometryMesh(ecs::RenderGeometryMesh* gpuMesh);

  private:
  // Map of GPU geometry meshes keyed by CPU mesh pointer
  std::unordered_map<ecs::Mesh*, std::unique_ptr<ecs::RenderGeometryMesh>> m_renderGeometryMeshes;
  mutable std::mutex                                                       m_mutex;
};

}  // namespace arise

#endif  // ARISE_RENDER_GEOMETRY_MESH_MANAGER_H