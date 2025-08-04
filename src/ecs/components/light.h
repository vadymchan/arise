#ifndef ARISE_LIGHT_H
#define ARISE_LIGHT_H

#include <math_library/vector.h>

namespace arise {
namespace ecs {

// TODO: consider adding a base class inside all light types(Directional, Point, Spot) for better cache locality
struct Light {
  math::Vector3f color;
  float          intensity;
  bool           isDirty = true;
  bool           enabled = true;
};

struct DirectionalLight {
  math::Vector3f direction;
  bool           isDirty = true;
};

struct PointLight {
  float range;
  bool  isDirty = true;
};

struct SpotLight {
  // This can be retrieved from the transform component
  // math::Vector3f direction;
  float range;
  float innerConeAngle;
  float outerConeAngle;
  bool  isDirty = true;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_LIGHT_H
