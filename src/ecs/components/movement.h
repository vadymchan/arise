#ifndef ARISE_MOVEMENT_H
#define ARISE_MOVEMENT_H

#include <math_library/vector.h>

namespace arise {
namespace ecs {

struct Movement {
  math::Vector3f direction;
  float           strength;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_MOVEMENT_H
