#ifndef ARISE_MODEL_TYPES_H
#define ARISE_MODEL_TYPES_H

#include <algorithm>
#include <string>
#include <unordered_map>

namespace arise {

enum class ModelType {
  OBJ,
  FBX,
  GLTF,
  GLB,
  UNKNOWN,
};

ModelType getModelTypeFromExtension(const std::string& extension);

}  // namespace arise

#endif  // ARISE_MODEL_TYPES_H
