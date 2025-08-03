#ifdef ARISE_USE_CGLTF

#include "resources/cgltf/cgltf_render_model_loader.h"

#include "resources/cgltf/cgltf_common.h"
#include "resources/cgltf/cgltf_material_loader.h"
#include "resources/cgltf/cgltf_model_loader.h"
#include "utils/buffer/buffer_manager.h"
#include "utils/logger/global_logger.h"
#include "utils/material/material_manager.h"
#include "utils/model/mesh_manager.h"
#include "utils/model/model_manager.h"
#include "utils/model/render_geometry_mesh_manager.h"
#include "utils/model/render_mesh_manager.h"
#include "utils/service/service_locator.h"

#include <cgltf.h>
#include <math_library/matrix.h>

namespace arise {

std::unique_ptr<ecs::RenderModel> CgltfRenderModelLoader::loadRenderModel(const std::filesystem::path& filePath,
                                                                     ecs::Model**                      outModelPtr) {
  auto scene = CgltfSceneCache::getOrLoad(filePath);
  if (!scene) {
    GlobalLogger::Log(LogLevel::Error, "Failed to load GLTF scene for material mapping: " + filePath.string());
    return nullptr;
  }

  const cgltf_data* data = scene.get();

  auto materialManager           = ServiceLocator::s_get<MaterialManager>();
  auto renderGeometryMeshManager = ServiceLocator::s_get<RenderGeometryMeshManager>();
  auto renderMeshManager         = ServiceLocator::s_get<RenderMeshManager>();
  auto bufferManager             = ServiceLocator::s_get<BufferManager>();
  auto modelManager              = ServiceLocator::s_get<ModelManager>();

  if (!modelManager) {
    GlobalLogger::Log(LogLevel::Error, "ModelManager not available in ServiceLocator while loading: " + filePath.string());
    return nullptr;
  }

  auto cpuModelPtr = modelManager->getModel(filePath);
  if (outModelPtr) {
    *outModelPtr = cpuModelPtr;
  }

  if (!materialManager || !renderGeometryMeshManager || !renderMeshManager || !bufferManager) {
    GlobalLogger::Log(LogLevel::Warning,
                      "GPU managers not available – loaded CPU model only for: " + filePath.string());
    return nullptr;  
  }

  auto materialPointers = materialManager->getMaterials(filePath);

  auto renderModel      = std::make_unique<ecs::RenderModel>();
  renderModel->filePath = filePath;

  auto& meshes = cpuModelPtr->meshes;
  renderModel->renderMeshes.reserve(meshes.size());

  size_t meshIndex = 0;
  for (size_t i = 0; i < data->meshes_count && meshIndex < meshes.size(); ++i) {
    cgltf_mesh* gltf_mesh = &data->meshes[i];

    for (size_t j = 0; j < gltf_mesh->primitives_count && meshIndex < meshes.size(); ++j) {
      ecs::Mesh* meshPtr = meshes[meshIndex++];

      auto renderGeometryMesh = createRenderGeometryMesh(meshPtr);
      if (!renderGeometryMesh || !renderGeometryMesh->vertexBuffer || !renderGeometryMesh->indexBuffer) {
        GlobalLogger::Log(LogLevel::Warning,
                          "Failed to create GPU geometry buffers for mesh " + meshPtr->meshName
                              + ". Falling back to CPU-only model.");
        return nullptr;
      }

      auto gpuMeshPtr = renderGeometryMeshManager->addRenderGeometryMesh(std::move(renderGeometryMesh), meshPtr);
      if (!gpuMeshPtr) {
        GlobalLogger::Log(LogLevel::Warning,
                          "Failed to register RenderGeometryMesh for mesh " + meshPtr->meshName
                              + ". Falling back to CPU-only model.");
        return nullptr;
      }

      ecs::Material*         materialPtr = nullptr;
      const cgltf_primitive* primitive   = &gltf_mesh->primitives[j];
      if (primitive->material) {
        for (size_t materialIndex = 0; materialIndex < data->materials_count; ++materialIndex) {
          if (&data->materials[materialIndex] == primitive->material) {
            if (materialIndex < materialPointers.size()) {
              materialPtr = materialPointers[materialIndex];
            } else {
              GlobalLogger::Log(
                  LogLevel::Warning,
                  "Material index " + std::to_string(materialIndex) + " out of range for mesh " + meshPtr->meshName);
            }
            break;
          }
        }
      }

      auto renderMeshPtr = renderMeshManager->addRenderMesh(gpuMeshPtr, materialPtr, meshPtr);

      std::string bufferName = "transform_matrix_" + meshPtr->meshName;
      if (bufferManager) {
        renderMeshPtr->transformMatrixBuffer
            = bufferManager->createUniformBuffer(sizeof(math::Matrix4f<>), &meshPtr->transformMatrix, bufferName);
        if (renderMeshPtr->transformMatrixBuffer) {
          GlobalLogger::Log(LogLevel::Debug, "Created transform matrix buffer for mesh " + meshPtr->meshName);
        } else {
          GlobalLogger::Log(LogLevel::Warning,
                            "Failed to create transform matrix buffer for mesh " + meshPtr->meshName);
        }
      }

      renderModel->renderMeshes.push_back(renderMeshPtr);
    }
  }

  if (outModelPtr) {
    *outModelPtr = cpuModelPtr;
    GlobalLogger::Log(LogLevel::Debug, "CPU model pointer provided to caller: " + filePath.string());
  }

  return renderModel;
}

std::unique_ptr<ecs::RenderGeometryMesh> CgltfRenderModelLoader::createRenderGeometryMesh(ecs::Mesh* mesh) {
  auto renderGeometryMesh = std::make_unique<ecs::RenderGeometryMesh>();

  renderGeometryMesh->vertexBuffer = createVertexBuffer(mesh);
  renderGeometryMesh->indexBuffer  = createIndexBuffer(mesh);

  return renderGeometryMesh;
}

gfx::rhi::Buffer* CgltfRenderModelLoader::createVertexBuffer(const ecs::Mesh* mesh) {
  auto bufferManager = ServiceLocator::s_get<BufferManager>();
  if (!bufferManager) {
    GlobalLogger::Log(LogLevel::Error, "Cannot create vertex buffer, BufferManager not found");
    return nullptr;
  }

  std::string bufferName  = "VertexBuffer_";
  bufferName             += mesh->meshName.empty() ? "Unnamed" : mesh->meshName;

  return bufferManager->createVertexBuffer(mesh->vertices.data(), mesh->vertices.size(), sizeof(ecs::Vertex), bufferName);
}

gfx::rhi::Buffer* CgltfRenderModelLoader::createIndexBuffer(const ecs::Mesh* mesh) {
  auto bufferManager = ServiceLocator::s_get<BufferManager>();
  if (!bufferManager) {
    GlobalLogger::Log(LogLevel::Error, "Cannot create index buffer, BufferManager not found");
    return nullptr;
  }

  std::string bufferName  = "IndexBuffer_";
  bufferName             += mesh->meshName.empty() ? "Unnamed" : mesh->meshName;

  return bufferManager->createIndexBuffer(mesh->indices.data(), mesh->indices.size(), sizeof(uint32_t), bufferName);
}

}  // namespace arise

#endif  // ARISE_USE_CGLTF