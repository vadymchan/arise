#ifndef ARISE_VERTEX_H
#define ARISE_VERTEX_H

#include <math_library/vector.h>

namespace arise {
namespace ecs {

struct Vertex {
  math::Vector3f position;
  math::Vector2f texCoords;
  math::Vector3f normal;
  math::Vector3f tangent;
  math::Vector3f bitangent;
  math::Vector4f color;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_VERTEX_H
