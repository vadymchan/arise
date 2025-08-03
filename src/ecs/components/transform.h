#ifndef ARISE_TRANSFORM_H
#define ARISE_TRANSFORM_H

#include <math_library/graphics.h>
#include <math_library/quaternion.h>

namespace arise {
namespace ecs {

struct Transform {
  // translation is position relative to the some origin (world origin, parent
  // object, etc.)
  math::Vector3f translation;
  // rotation is Euler angles in degrees
  math::Vector3f rotation;
  math::Vector3f scale = math::Vector3f(1.0f, 1.0f, 1.0f);

  bool isDirty = false;
};

inline math::Matrix4f<> calculateTransformMatrix(const Transform& transform) {
  math::Matrix4f<> transformMatrix;

  // 1: Scale
  transformMatrix = math::g_scale(transform.scale);

  // 2: Rotation (get rotation matrix from quaternion that is transformed from Euler angles)
  auto pitch = transform.rotation.x();
  auto yaw   = transform.rotation.y();
  auto roll  = transform.rotation.z();
  pitch      = math::g_degreeToRadian(pitch);
  yaw        = math::g_degreeToRadian(yaw);
  roll       = math::g_degreeToRadian(roll);
  // auto q              = math::Quaternionf::fromEulerAngles(roll, pitch, yaw, math::EulerRotationOrder::ZXY);
  auto q              = math::Quaternionf::fromEulerAngles(pitch, yaw, roll, math::EulerRotationOrder::XYZ);
  auto rotationMatrix = math::Matrix4f<>::Identity();
  {
    // TODO: currently there's no way to convert from math::Matrix3f<> to
    // math::Matrix4f<> in math_library
    auto temp = q.toRotationMatrix();
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 3; ++j) {
        rotationMatrix(i, j) = temp(i, j);
      }
    }
  }
  transformMatrix *= rotationMatrix;

  // 3: Translation
  math::g_addTranslate(transformMatrix, transform.translation);

  return transformMatrix;
}

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_TRANSFORM_H
