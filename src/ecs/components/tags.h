#ifndef ARISE_TAGS_H
#define ARISE_TAGS_H

#include <filesystem>

namespace arise {
namespace ecs {

struct ModelLoadingTag {
  std::filesystem::path modelPath;
};

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_TAGS_H