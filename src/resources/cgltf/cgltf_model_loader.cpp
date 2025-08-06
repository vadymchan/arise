#ifdef ARISE_USE_CGLTF

#include "resources/cgltf/cgltf_model_loader.h"

#include "ecs/components/bounding_volume.h"
#include "ecs/components/model.h"
#include "resources/cgltf/cgltf_common.h"
#include "utils/logger/log.h"
#include "utils/model/mesh_manager.h"
#include "utils/service/service_locator.h"

#include <cgltf.h>
#include <math_library/graphics.h>
#include <math_library/quaternion.h>

#ifdef ARISE_USE_MIKKTS
#include <mikktspace.h>
#endif

namespace arise {

std::unique_ptr<ecs::Model> CgltfModelLoader::loadModel(const std::filesystem::path& filePath) {
  auto scene = CgltfSceneCache::getOrLoad(filePath);
  if (!scene) {
    LOG_ERROR("Failed to load GLTF scene: " + filePath.string());
    return nullptr;
  }
  const cgltf_data* data = scene.get();

  auto model      = std::make_unique<ecs::Model>();
  model->filePath = filePath;

  model->boundingBox = ecs::bounds::createInvalid();

  std::vector<ecs::BoundingBox> meshBoundingBoxes;

  auto meshManager = ServiceLocator::s_get<MeshManager>();
  if (!meshManager) {
    LOG_ERROR("MeshManager not available in ServiceLocator.");
    return nullptr;
  }

  for (size_t i = 0; i < data->meshes_count; ++i) {
    cgltf_mesh* gltf_mesh = &data->meshes[i];

    cgltf_node* meshNode = nullptr;
    for (size_t j = 0; j < data->nodes_count; ++j) {
      if (data->nodes[j].mesh == gltf_mesh) {
        meshNode = &data->nodes[j];
        break;
      }
    }

    for (size_t j = 0; j < gltf_mesh->primitives_count; ++j) {
      auto mesh = processPrimitive(&gltf_mesh->primitives[j]);
      if (mesh) {
        if (gltf_mesh->name) {
          mesh->meshName = gltf_mesh->name;
          if (gltf_mesh->primitives_count > 1) {
            mesh->meshName += "_primitive_" + std::to_string(j);
          }
        } else {
          mesh->meshName = "Mesh_" + std::to_string(i) + "_Primitive_" + std::to_string(j);
        }

        if (meshNode) {
          math::Matrix4f<> localMatrix = getNodeTransformMatrix(meshNode);
          math::Matrix4f<> worldMatrix = calculateWorldMatrix(meshNode, localMatrix);
          auto             zFlipMatrix = math::g_scale(math::Vector3f(1.0f, 1.0f, -1.0f));
          worldMatrix                  = worldMatrix * zFlipMatrix;  // Flip Z axis to match OpenGL coordinate system

          mesh->transformMatrix = worldMatrix;

          if (ecs::bounds::isValid(mesh->boundingBox)) {
            ecs::BoundingBox transformedBounds = ecs::bounds::transformAABB(mesh->boundingBox, mesh->transformMatrix);
            meshBoundingBoxes.push_back(transformedBounds);

            LOG_DEBUG("Mesh '" + mesh->meshName + "' bounding box: min(" + std::to_string(mesh->boundingBox.min.x())
                      + ", " + std::to_string(mesh->boundingBox.min.y()) + ", "
                      + std::to_string(mesh->boundingBox.min.z()) + ") max(" + std::to_string(mesh->boundingBox.max.x())
                      + ", " + std::to_string(mesh->boundingBox.max.y()) + ", "
                      + std::to_string(mesh->boundingBox.max.z()) + ")");
          }
        }

        auto* meshPtr = meshManager->addMesh(std::move(mesh), filePath);
        model->meshes.push_back(meshPtr);
      }
    }
  }

  if (!meshBoundingBoxes.empty()) {
    model->boundingBox = ecs::bounds::combineAABBs(meshBoundingBoxes);
    LOG_INFO("Model '" + filePath.filename().string() + "' combined bounding box calculated from "
             + std::to_string(meshBoundingBoxes.size()) + " meshes");
  } else {
    LOG_WARN("Model '" + filePath.filename().string() + "' has no valid bounding boxes");
  }

  return model;
}

math::Matrix4f<> CgltfModelLoader::calculateWorldMatrix(cgltf_node* node, const math::Matrix4f<>& localMatrix) {
  math::Matrix4f<> worldMatrix = localMatrix;

  cgltf_node* parent = node->parent;
  while (parent) {
    math::Matrix4f<> parentMatrix = getNodeTransformMatrix(parent);
    worldMatrix                   = worldMatrix * parentMatrix;
    parent                        = parent->parent;
  }

  return worldMatrix;
}

bool CgltfModelLoader::containsMesh(cgltf_node* node) {
  if (!node) {
    return false;
  }

  if (node->mesh) {
    return true;
  }

  for (size_t i = 0; i < node->children_count; ++i) {
    if (containsMesh(node->children[i])) {
      return true;
    }
  }

  return false;
}

math::Matrix4f<> CgltfModelLoader::getNodeTransformMatrix(const cgltf_node* node) {
  if (node->has_matrix) {
    math::Matrix4f<> matrix = math::Matrix4f<>::Identity();
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        matrix(row, col) = node->matrix[row * 4 + col];
      }
    }
    return matrix;
  } else {
    math::Vector3f    translation(0.0f, 0.0f, 0.0f);
    math::Quaternionf rotation(0.0f, 0.0f, 0.0f, 1.0f);
    math::Vector3f    scale(1.0f, 1.0f, 1.0f);

    if (node->has_translation) {
      translation = math::Vector3f(node->translation[0], node->translation[1], node->translation[2]);
    }

    if (node->has_rotation) {
      rotation = math::Quaternionf(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
    }

    if (node->has_scale) {
      scale = math::Vector3f(node->scale[0], node->scale[1], node->scale[2]);
    }

    math::Matrix4f<> scaleMatrix       = math::g_scale(scale);
    math::Matrix4f<> translationMatrix = math::g_translate(translation);
    math::Matrix4f<> rotationMatrix    = math::Matrix4f<>::Identity();
    {
      // TODO: need a functionality to convert from math::Matrix3f<> to math::Matrix4f<> (math_library propose)
      auto temp = rotation.toRotationMatrix();
      for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
          rotationMatrix(i, j) = temp(i, j);
        }
      }
    }

    // T * R * S (but in row major order)
    math::Matrix4f<> transformMatrix = scaleMatrix * rotationMatrix * translationMatrix;

    return transformMatrix;
  }
}

std::unique_ptr<ecs::Mesh> CgltfModelLoader::processPrimitive(const cgltf_primitive* primitive) {
  auto mesh = std::make_unique<ecs::Mesh>();

  if (primitive->type != cgltf_primitive_type_triangles) {
    LOG_WARN("Skipping non-triangle primitive");
    return nullptr;
  }

  processVertices(primitive, mesh.get());
  processIndices(primitive, mesh.get());
  processBoundingBox(mesh.get(), primitive);

  bool hasTangents = false;
  for (size_t i = 0; i < primitive->attributes_count; ++i) {
    if (primitive->attributes[i].type == cgltf_attribute_type_tangent) {
      hasTangents = true;
      break;
    }
  }

  if (!hasTangents && !mesh->vertices.empty() && !mesh->indices.empty()) {
#ifdef ARISE_USE_MIKKTS
    generateMikkTSpaceTangents(mesh.get());
#else
    calculateTangents(mesh.get());
#endif
  }

  return mesh;
}

void CgltfModelLoader::processVertices(const cgltf_primitive* primitive, ecs::Mesh* mesh) {
  const cgltf_accessor* position_accessor = nullptr;
  const cgltf_accessor* normal_accessor   = nullptr;
  const cgltf_accessor* texcoord_accessor = nullptr;
  const cgltf_accessor* tangent_accessor  = nullptr;
  const cgltf_accessor* color_accessor    = nullptr;

  for (size_t i = 0; i < primitive->attributes_count; ++i) {
    const cgltf_attribute* attribute = &primitive->attributes[i];

    if (attribute->type == cgltf_attribute_type_position) {
      position_accessor = attribute->data;
    } else if (attribute->type == cgltf_attribute_type_normal) {
      normal_accessor = attribute->data;
    } else if (attribute->type == cgltf_attribute_type_texcoord && attribute->index == 0) {
      texcoord_accessor = attribute->data;
    } else if (attribute->type == cgltf_attribute_type_tangent) {
      tangent_accessor = attribute->data;
    } else if (attribute->type == cgltf_attribute_type_color && attribute->index == 0) {
      color_accessor = attribute->data;
    }
  }

  if (!position_accessor) {
    LOG_ERROR("No position data found in primitive");
    return;
  }

  size_t vertex_count = position_accessor->count;
  mesh->vertices.reserve(vertex_count);

  for (size_t i = 0; i < vertex_count; ++i) {
    ecs::Vertex vertex;

    float position[3] = {0, 0, 0};
    cgltf_accessor_read_float(position_accessor, i, position, 3);
    vertex.position = math::Vector3f(position[0], position[1], position[2]);

    if (normal_accessor) {
      float normal[3] = {0, 0, 0};
      cgltf_accessor_read_float(normal_accessor, i, normal, 3);
      vertex.normal = math::Vector3f(normal[0], normal[1], normal[2]);
    } else {
      vertex.normal = math::Vector3f(0.0f, 0.0f, 0.0f);
    }

    if (texcoord_accessor) {
      float texcoord[2] = {0, 0};
      cgltf_accessor_read_float(texcoord_accessor, i, texcoord, 2);
      vertex.texCoords = math::Vector2f(texcoord[0], texcoord[1]);
    } else {
      vertex.texCoords = math::Vector2f(0.0f, 0.0f);
    }

    if (tangent_accessor) {
      float tangent[4] = {0, 0, 0, 1};  // xyzw, w is handedness
      cgltf_accessor_read_float(tangent_accessor, i, tangent, 4);
      vertex.tangent = math::Vector3f(tangent[0], tangent[1], tangent[2]);

      vertex.bitangent = vertex.normal.cross(vertex.tangent) * tangent[3];
    } else {
      vertex.tangent   = math::Vector3f(0.0f, 0.0f, 0.0f);
      vertex.bitangent = math::Vector3f(0.0f, 0.0f, 0.0f);
    }

    if (color_accessor) {
      float color[4] = {1, 1, 1, 1};
      cgltf_accessor_read_float(color_accessor, i, color, 4);
      vertex.color = math::Vector4f(color[0], color[1], color[2], color[3]);
    } else {
      vertex.color = math::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    mesh->vertices.push_back(vertex);
  }
}

void CgltfModelLoader::processIndices(const cgltf_primitive* primitive, ecs::Mesh* mesh) {
  if (primitive->indices) {
    const cgltf_accessor* indices     = primitive->indices;
    size_t                index_count = indices->count;
    mesh->indices.reserve(index_count);

    for (size_t i = 0; i < index_count; ++i) {
      uint32_t index = static_cast<uint32_t>(cgltf_accessor_read_index(indices, i));
      mesh->indices.push_back(index);
    }
  } else if (!mesh->vertices.empty()) {
    // If no indices are provided, generate them (0, 1, 2, ...)
    mesh->indices.reserve(mesh->vertices.size());
    for (size_t i = 0; i < mesh->vertices.size(); ++i) {
      mesh->indices.push_back(static_cast<uint32_t>(i));
    }
  }
}

// TODO: the same implementation is in asssimp. Consider moving it to a common place and use in both loaders.
void CgltfModelLoader::calculateTangents(ecs::Mesh* mesh) {
  if (mesh->indices.size() % 3 != 0) {
    LOG_WARN("Index count is not a multiple of 3, cannot calculate tangents");
    return;
  }

  for (size_t i = 0; i < mesh->indices.size(); i += 3) {
    uint32_t i0 = mesh->indices[i];
    uint32_t i1 = mesh->indices[i + 1];
    uint32_t i2 = mesh->indices[i + 2];

    ecs::Vertex& v0 = mesh->vertices[i0];
    ecs::Vertex& v1 = mesh->vertices[i1];
    ecs::Vertex& v2 = mesh->vertices[i2];

    math::Vector3f edge1 = v1.position - v0.position;
    math::Vector3f edge2 = v2.position - v0.position;

    math::Vector2f deltaUV1 = v1.texCoords - v0.texCoords;
    math::Vector2f deltaUV2 = v2.texCoords - v0.texCoords;

    float f = 1.0f / (deltaUV1.x() * deltaUV2.y() - deltaUV2.x() * deltaUV1.y());
    if (!std::isfinite(f)) {
      f = 0.0f;  // Handle degenerate triangles
    }

    math::Vector3f tangent;
    tangent.x() = f * (deltaUV2.y() * edge1.x() - deltaUV1.y() * edge2.x());
    tangent.y() = f * (deltaUV2.y() * edge1.y() - deltaUV1.y() * edge2.y());
    tangent.z() = f * (deltaUV2.y() * edge1.z() - deltaUV1.y() * edge2.z());
    tangent     = tangent.normalized();

    math::Vector3f bitangent;
    bitangent.x() = f * (-deltaUV2.x() * edge1.x() + deltaUV1.x() * edge2.x());
    bitangent.y() = f * (-deltaUV2.x() * edge1.y() + deltaUV1.x() * edge2.y());
    bitangent.z() = f * (-deltaUV2.x() * edge1.z() + deltaUV1.x() * edge2.z());
    bitangent     = bitangent.normalized();

    v0.tangent = tangent;
    v1.tangent = tangent;
    v2.tangent = tangent;

    v0.bitangent = bitangent;
    v1.bitangent = bitangent;
    v2.bitangent = bitangent;
  }

  LOG_DEBUG("Basic tangent calculation completed");
}

#ifdef ARISE_USE_MIKKTS

struct MikkTSpaceContext {
  ecs::Mesh* mesh;
};

static int getNumFaces(const SMikkTSpaceContext* context) {
  MikkTSpaceContext* userContext = static_cast<MikkTSpaceContext*>(context->m_pUserData);
  return static_cast<int>(userContext->mesh->indices.size() / 3);
}

static int getNumVerticesOfFace(const SMikkTSpaceContext* context, int faceIdx) {
  return 3;  // Always triangles
}

static void getPosition(const SMikkTSpaceContext* context, float outpos[], int faceIdx, int vertIdx) {
  MikkTSpaceContext*    userContext = static_cast<MikkTSpaceContext*>(context->m_pUserData);
  int                   index       = userContext->mesh->indices[faceIdx * 3 + vertIdx];
  const math::Vector3f& pos         = userContext->mesh->vertices[index].position;
  outpos[0]                         = pos.x();
  outpos[1]                         = pos.y();
  outpos[2]                         = pos.z();
}

static void getNormal(const SMikkTSpaceContext* context, float outnormal[], int faceIdx, int vertIdx) {
  MikkTSpaceContext*    userContext = static_cast<MikkTSpaceContext*>(context->m_pUserData);
  int                   index       = userContext->mesh->indices[faceIdx * 3 + vertIdx];
  const math::Vector3f& normal      = userContext->mesh->vertices[index].normal;
  outnormal[0]                      = normal.x();
  outnormal[1]                      = normal.y();
  outnormal[2]                      = normal.z();
}

static void getTexCoord(const SMikkTSpaceContext* context, float outuv[], int faceIdx, int vertIdx) {
  MikkTSpaceContext*    userContext = static_cast<MikkTSpaceContext*>(context->m_pUserData);
  int                   index       = userContext->mesh->indices[faceIdx * 3 + vertIdx];
  const math::Vector2f& uv          = userContext->mesh->vertices[index].texCoords;
  outuv[0]                          = uv.x();
  outuv[1]                          = uv.y();
}

static void setTangent(const SMikkTSpaceContext* context, const float tangent[], float sign, int faceIdx, int vertIdx) {
  MikkTSpaceContext* userContext = static_cast<MikkTSpaceContext*>(context->m_pUserData);
  int                index       = userContext->mesh->indices[faceIdx * 3 + vertIdx];

  userContext->mesh->vertices[index].tangent = math::Vector3f(tangent[0], tangent[1], tangent[2]);

  const math::Vector3f& normal                 = userContext->mesh->vertices[index].normal;
  userContext->mesh->vertices[index].bitangent = normal.cross(userContext->mesh->vertices[index].tangent) * sign;
}

void CgltfModelLoader::generateMikkTSpaceTangents(ecs::Mesh* mesh) {
  if (mesh->vertices.empty() || mesh->indices.empty()) {
    return;
  }

  SMikkTSpaceInterface interface   = {};
  interface.m_getNumFaces          = getNumFaces;
  interface.m_getNumVerticesOfFace = getNumVerticesOfFace;
  interface.m_getPosition          = getPosition;
  interface.m_getNormal            = getNormal;
  interface.m_getTexCoord          = getTexCoord;
  interface.m_setTSpaceBasic       = setTangent;

  MikkTSpaceContext userContext = {mesh};

  SMikkTSpaceContext context = {};
  context.m_pInterface       = &interface;
  context.m_pUserData        = &userContext;

  if (!genTangSpaceDefault(&context)) {
    LOG_ERROR("Failed to generate tangents using MikkTSpace");
    calculateTangents(mesh);
  } else {
    LOG_DEBUG("MikkTSpace tangent generation completed");
  }
}
#endif  // ARISE_USE_MIKKTS

ecs::BoundingBox CgltfModelLoader::extractBoundingBoxFromAccessor(const cgltf_accessor* positionAccessor) {
  if (!positionAccessor || !positionAccessor->has_min || !positionAccessor->has_max) {
    return ecs::bounds::createInvalid();
  }

  ecs::BoundingBox box;
  box.min = math::Vector3f(static_cast<float>(positionAccessor->min[0]),
                           static_cast<float>(positionAccessor->min[1]),
                           static_cast<float>(positionAccessor->min[2]));

  box.max = math::Vector3f(static_cast<float>(positionAccessor->max[0]),
                           static_cast<float>(positionAccessor->max[1]),
                           static_cast<float>(positionAccessor->max[2]));

  return box;
}

ecs::BoundingBox CgltfModelLoader::calculateBoundingBoxFromVertices(const ecs::Mesh* mesh) {
  if (!mesh || mesh->vertices.empty()) {
    LOG_WARN("Cannot calculate bounding box from empty mesh");
    return ecs::bounds::createInvalid();
  }

  return ecs::bounds::calculateAABB(mesh->vertices);
}

void CgltfModelLoader::processBoundingBox(ecs::Mesh* mesh, const cgltf_primitive* primitive) {
  if (!mesh || !primitive) {
    return;
  }

  const cgltf_accessor* positionAccessor = nullptr;
  for (size_t i = 0; i < primitive->attributes_count; ++i) {
    const cgltf_attribute* attribute = &primitive->attributes[i];
    if (attribute->type == cgltf_attribute_type_position) {
      positionAccessor = attribute->data;
      break;
    }
  }

  if (positionAccessor) {
    mesh->boundingBox = extractBoundingBoxFromAccessor(positionAccessor);

    if (ecs::bounds::isValid(mesh->boundingBox)) {
      LOG_DEBUG("Bounding box extracted from GLTF accessor data");
      return;
    }
  }

  mesh->boundingBox = calculateBoundingBoxFromVertices(mesh);

  if (ecs::bounds::isValid(mesh->boundingBox)) {
    LOG_DEBUG("Bounding box calculated from vertex data");
  } else {
    LOG_WARN("Failed to calculate valid bounding box for mesh");
  }
}

}  // namespace arise

#endif  // ARISE_USE_CGLTF