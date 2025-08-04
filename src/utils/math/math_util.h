#ifndef ARISE_MATH_UTIL_H
#define ARISE_MATH_UTIL_H

#include "config/config_manager.h"

#include <math_library/graphics.h>
#include <math_library/matrix.h>
#include <math_library/quaternion.h>
#include <math_library/vector.h>

// TODO: consider move this code to math_library third party
namespace math {

// TODO:
// - add functions like get matrix w/o scale/translate/rotate component
// - add functions like make scale + translate (and similar)

// ----------------------------------------------------------------------------

inline float normalizeAngle(float angle) {
  // Get the angle to be within the range [0, 360)
  angle = std::fmod(angle, 360.0f);

  // Convert to range [-180, 180)
  if (angle >= 180.0f) {
    angle -= 360.0f;
  } else if (angle < -180.0f) {
    angle += 360.0f;
  }

  return angle;
}

inline math::Vector3f normalizeRotation(const math::Vector3f& rotation) {
  return math::Vector3f(normalizeAngle(rotation.x()), normalizeAngle(rotation.y()), normalizeAngle(rotation.z()));
}

Vector3f g_getVectorfromConfig(const arise::ConfigValue& value);

}  // namespace math

#endif  // ARISE_MATH_UTIL_H
