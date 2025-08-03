#ifndef ARISE_CAMERA_H
#define ARISE_CAMERA_H

#include <math_library/matrix.h>

namespace arise {
namespace ecs {

enum class CameraType {
  Perspective  = 0,
  Orthographic = 1
};

struct Camera {
  CameraType type;
  float      fov;
  float      nearClip;
  float      farClip;
  float      width;
  float      height;
};

struct CameraMatrices {
  math::Matrix4f<> view;
  math::Matrix4f<> projection;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_CAMERA_H
