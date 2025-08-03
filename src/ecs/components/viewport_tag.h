#ifndef ARISE_VIEWPORT_TAG_H
#define ARISE_VIEWPORT_TAG_H
#include <string>

namespace arise {
namespace ecs {
struct ViewportTag {
  std::string id = "main";
};
}  // namespace ecs
}  // namespace arise

#endif  // ARISE_VIEWPORT_TAG_H