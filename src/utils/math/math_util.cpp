#include "utils/math/math_util.h"

#include "utils/logger/log.h"

namespace math {

Vector3f g_getVectorfromConfig(const arise::ConfigValue& value) {
  Vector3f result;

  result.x() = value["x"].GetFloat();
  result.y() = value["y"].GetFloat();
  result.z() = value["z"].GetFloat();

  return result;
}

}  // namespace math
