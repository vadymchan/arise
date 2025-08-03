#ifndef ARISE_SELECTED_H
#define ARISE_SELECTED_H

#include <math_library/vector.h>

namespace arise {
namespace ecs {

struct Selected {
  math::Vector4f highlightColor   = math::Vector4f(1.0f, 0.5f, 0.0f, 1.0f);  // Orange
  float           outlineThickness = 0.02f;
  bool            xRay             = false;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_SELECTED_H