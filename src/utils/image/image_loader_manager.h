#ifndef ARISE_IMAGE_LOADER_MANAGER_H
#define ARISE_IMAGE_LOADER_MANAGER_H

#include "file_loader/image_file_loader.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace arise {

class ImageLoaderManager {
  public:
  ImageLoaderManager() = default;

  void registerLoader(ImageType imageType, std::shared_ptr<IImageLoader> loader);

  std::unique_ptr<Image> loadImage(const std::filesystem::path& filepath);

  private:
  std::unordered_map<ImageType, std::shared_ptr<IImageLoader>> loaderMap_;
  std::mutex                                                   mutex_;
};

}  // namespace arise

#endif  // ARISE_IMAGE_LOADER_MANAGER_H