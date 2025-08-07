#include "utils/image/image_loader_manager.h"

#include "utils/logger/log.h"

namespace arise {

void ImageLoaderManager::registerLoader(ImageType imageType, std::shared_ptr<IImageLoader> loader) {
  std::lock_guard<std::mutex> lock(mutex_);
  loaderMap_[imageType] = std::move(loader);
}

std::unique_ptr<Image> ImageLoaderManager::loadImage(const std::filesystem::path& filepath) {
  std::string extension = filepath.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
  ImageType imageType = getImageTypeFromExtension(extension);

  if (imageType == ImageType::UNKNOWN) {
    LOG_ERROR("Unknown image type for extension: {}", extension);
    return nullptr;
  }

  IImageLoader* loader = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto                        it = loaderMap_.find(imageType);
    if (it != loaderMap_.end()) {
      loader = it->second.get();
    }
  }

  if (loader && loader->supportsFormat(extension)) {
    return loader->loadImage(filepath);
  }

  LOG_ERROR("No suitable loader found for image type: {}", extension);
  return nullptr;
}

}  // namespace arise