#ifndef ARISE_GLTF_RENDER_MODEL_LOADER_H
#define ARISE_GLTF_RENDER_MODEL_LOADER_H

#ifdef ARISE_USE_CGLTF

#include "gfx/rhi/interface/device.h"
#include "resources/i_render_model_loader.h"

namespace arise {

class CgltfRenderModelLoader : public IRenderModelLoader {
  public:
  CgltfRenderModelLoader()  = default;
  ~CgltfRenderModelLoader() = default;

  std::unique_ptr<ecs::RenderModel> loadRenderModel(const std::filesystem::path& filePath,
                                                    ecs::Model**                 outModel = nullptr) override;

  private:
  // GPU-side geometry mesh
  std::unique_ptr<ecs::RenderGeometryMesh> createRenderGeometryMesh(ecs::Mesh* mesh);

  gfx::rhi::Buffer* createVertexBuffer(const ecs::Mesh* mesh);
  gfx::rhi::Buffer* createIndexBuffer(const ecs::Mesh* mesh);
};

}  // namespace arise

#endif  // ARISE_USE_CGLTF

#endif  // ARISE_GLTF_RENDER_MODEL_LOADER_H