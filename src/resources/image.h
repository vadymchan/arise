#ifndef ARISE_IMAGE_H
#define ARISE_IMAGE_H

#include "gfx/rhi/common/rhi_enums.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace arise {

struct SubImage {
  size_t width;
  size_t height;
  size_t rowPitch;    // width * height * bytes per pixel (also known as stride)
  size_t slicePitch;  // width * height * bytes per pixel
  size_t pixelOffset;
};

struct Image {
  size_t                  width;
  size_t                  height;
  size_t                  depth;      // For 3D textures; 1 for 2D textures
  size_t                  mipLevels;  // Number of mipmap levels; 1 if no mipmaps
  size_t                  arraySize;  // Number of array slices; 1 if not an array texture
  // TODO: consider renaming Texture... to something else (maybe PixelFormat and ImageDimension - alias to current
  // enums)
  gfx::rhi::TextureFormat format;
  gfx::rhi::TextureType   dimension;  // Texture dimension - 1D, 2D, 3D

  // Raw pixel data for all mip levels and array slices
  std::vector<std::byte> pixels;

  // Holds sub-images for each mip level / array slice
  std::vector<SubImage> subImages;
};

// TODO: consider moving to separate file (like image_type.h)
#include <algorithm>
#include <string>
#include <unordered_map>

enum class ImageType {
  JPEG,
  JPG,
  PNG,
  BMP,
  TGA,
  GIF,
  HDR,
  PIC,
  PPM,
  PGM,
  DDS,
  KTX,
  KTX2,
  UNKNOWN,
};

inline ImageType getImageTypeFromExtension(const std::string& extension) {
  static const std::unordered_map<std::string, ImageType> extensionToType = {
    {".jpeg", ImageType::JPEG},
    { ".jpg",  ImageType::JPG},
    { ".png",  ImageType::PNG},
    { ".bmp",  ImageType::BMP},
    { ".tga",  ImageType::TGA},
    { ".gif",  ImageType::GIF},
    { ".hdr",  ImageType::HDR},
    { ".pic",  ImageType::PIC},
    { ".ppm",  ImageType::PPM},
    { ".pgm",  ImageType::PGM},
    { ".dds",  ImageType::DDS},
    { ".ktx",  ImageType::KTX},
    {".ktx2", ImageType::KTX2},
  };

  std::string ext = extension;
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  auto it = extensionToType.find(ext);
  if (it != extensionToType.end()) {
    return it->second;
  }

  return ImageType::UNKNOWN;
}

}  // namespace arise

#endif  // ARISE_IMAGE_H
