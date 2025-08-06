#include "utils/third_party/stb_util.h"

#include "utils/logger/log.h"

#include <unordered_set>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

namespace arise {

const std::unordered_set<std::string> STBImageLoader::supportedExtensions_
    = {".jpeg", ".jpg", ".png", ".bmp", ".tga", ".gif", ".hdr", ".pic", ".ppm", ".pgm"};

bool STBImageLoader::supportsFormat(const std::string& extension) const {
  return supportedExtensions_.contains(extension);
}

std::unique_ptr<Image> STBImageLoader::loadImage(const std::filesystem::path& filepath) {
  if (stbi_is_hdr(filepath.string().c_str())) {
    return loadImageData_(filepath, &loadHdr_, stbi_image_free, 32, true);
  } else if (stbi_is_16_bit(filepath.string().c_str())) {
    return loadImageData_(filepath, &load16Bit_, stbi_image_free, 16, false);
  } else {
    return loadImageData_(filepath, &load8Bit_, stbi_image_free, 8, false);
  }
}

void* STBImageLoader::loadHdr_(
    const std::filesystem::path& filepath, int32_t* x, int32_t* y, int32_t* channelsInFile, int32_t desiredChannels) {
  return static_cast<void*>(stbi_loadf(filepath.string().c_str(), x, y, channelsInFile, desiredChannels));
}

void* STBImageLoader::load16Bit_(
    const std::filesystem::path& filepath, int32_t* x, int32_t* y, int32_t* channelsInFile, int32_t desiredChannels) {
  return static_cast<void*>(stbi_load_16(filepath.string().c_str(), x, y, channelsInFile, desiredChannels));
}

void* STBImageLoader::load8Bit_(
    const std::filesystem::path& filepath, int32_t* x, int32_t* y, int32_t* channelsInFile, int32_t desiredChannels) {
  return static_cast<void*>(stbi_load(filepath.string().c_str(), x, y, channelsInFile, desiredChannels));
}

gfx::rhi::TextureFormat STBImageLoader::determineFormat_(int32_t channels, int32_t bitsPerChannel, bool isHdr) {
  if (channels == 1) {
    if (bitsPerChannel == 8 && !isHdr) {
      return gfx::rhi::TextureFormat::R8;
    } else if (bitsPerChannel == 16 && !isHdr) {
      return gfx::rhi::TextureFormat::R16f;
    } else if (bitsPerChannel == 32 || isHdr) {
      return gfx::rhi::TextureFormat::R32f;
    }
  } else if (channels == 2) {
    if (bitsPerChannel == 8 && !isHdr) {
      return gfx::rhi::TextureFormat::Rg8;
    } else if (bitsPerChannel == 16 && !isHdr) {
      return gfx::rhi::TextureFormat::Rg16f;
    } else if (bitsPerChannel == 32 || isHdr) {
      return gfx::rhi::TextureFormat::Rg32f;
    }
  } else if (channels == 3) {
    if (bitsPerChannel == 8 && !isHdr) {
      return gfx::rhi::TextureFormat::Rgb8;
    } else if (bitsPerChannel == 16 && !isHdr) {
      return gfx::rhi::TextureFormat::Rgb16f;
    } else if (bitsPerChannel == 32 || isHdr) {
      return gfx::rhi::TextureFormat::Rgb32f;
    }
  } else if (channels == 4) {
    if (bitsPerChannel == 8 && !isHdr) {
      return gfx::rhi::TextureFormat::Rgba8;
    } else if (bitsPerChannel == 16 && !isHdr) {
      return gfx::rhi::TextureFormat::Rgba16f;
    } else if (bitsPerChannel == 32 || isHdr) {
      return gfx::rhi::TextureFormat::Rgba32f;
    }
  }

  LOG_ERROR("Unknown format. channels = " + std::to_string(channels)
            + ", bitsPerChannel = " + std::to_string(bitsPerChannel) + ", isHdr = " + std::to_string(isHdr));
  return gfx::rhi::TextureFormat::Count;
}

std::unique_ptr<Image> STBImageLoader::loadImageData_(
    const std::filesystem::path& filepath, LoaderFunc loader, FreeFunc freeFunc, int32_t bitsPerChannel, bool isHdr) {
  int32_t           width           = 0;
  int32_t           height          = 0;
  int32_t           channelsInFile  = 0;
  constexpr int32_t desiredChannels = 4;  // always decode as RGBA

  void* data = loader(filepath, &width, &height, &channelsInFile, desiredChannels);
  if (!data) {
    LOG_ERROR("Failed to load image: " + filepath.string());
    return nullptr;
  }

  const size_t bytesPerChannel = static_cast<size_t>(bitsPerChannel) / 8;
  const size_t bytesPerPixel   = bytesPerChannel * desiredChannels;
  const size_t imageSize       = static_cast<size_t>(width) * height * bytesPerPixel;

  std::vector<std::byte> pixels(reinterpret_cast<std::byte*>(data), reinterpret_cast<std::byte*>(data) + imageSize);
  freeFunc(data);

  auto image       = std::make_unique<Image>();
  image->width     = static_cast<size_t>(width);
  image->height    = static_cast<size_t>(height);
  image->depth     = 1;
  image->mipLevels = 1;
  image->arraySize = 1;
  image->dimension = gfx::rhi::TextureType::Texture2D;
  image->format    = (bitsPerChannel == 8)  ? gfx::rhi::TextureFormat::Rgba8
                   : (bitsPerChannel == 16) ? gfx::rhi::TextureFormat::Rgba16f
                                            : gfx::rhi::TextureFormat::Rgba32f;
  image->pixels    = std::move(pixels);

  SubImage baseSub;
  baseSub.width       = image->width;
  baseSub.height      = image->height;
  baseSub.rowPitch    = image->width * bytesPerPixel;
  baseSub.slicePitch  = baseSub.rowPitch * image->height;
  baseSub.pixelOffset = 0;
  image->subImages.push_back(baseSub);

  LOG_DEBUG("Loaded " + filepath.string() + " (" + std::to_string(width) + "x" + std::to_string(height) + ", RGBA, "
            + std::to_string(bitsPerChannel) + " bpc)");

  generateMipmaps_(image, desiredChannels, bitsPerChannel);
  return image;
}

void STBImageLoader::generateMipmaps_(std::unique_ptr<Image>& image, int32_t channels, int32_t bitsPerChannel) {
  const size_t channelsCount   = static_cast<size_t>(channels);
  const size_t bytesPerChannel = bitsPerChannel / 8;
  const size_t bytesPerPixel   = channelsCount * bytesPerChannel;

  size_t                 prevWidth  = image->width;
  size_t                 prevHeight = image->height;
  std::vector<std::byte> prevPixels = std::move(image->pixels);

  std::vector<std::byte> allPixels;
  std::vector<SubImage>  newSubImages;
  size_t                 offset = 0;

  stbir_datatype dataType;
  if (bitsPerChannel == 8) {
    dataType = STBIR_TYPE_UINT8;
  } else if (bitsPerChannel == 16) {
    dataType = STBIR_TYPE_UINT16;
  } else {
    dataType = STBIR_TYPE_FLOAT;
  }

  while (true) {
    allPixels.insert(allPixels.end(), prevPixels.begin(), prevPixels.end());

    SubImage subImage;
    subImage.width       = prevWidth;
    subImage.height      = prevHeight;
    subImage.rowPitch    = prevWidth * bytesPerPixel;
    subImage.slicePitch  = subImage.rowPitch * prevHeight;
    subImage.pixelOffset = offset;
    newSubImages.push_back(subImage);

    offset += prevPixels.size();
    if (prevWidth == 1 && prevHeight == 1) {
      break;
    }

    size_t                 nextW = prevWidth > 1 ? prevWidth / 2 : 1;
    size_t                 nextH = prevHeight > 1 ? prevHeight / 2 : 1;
    std::vector<std::byte> nextPixels(nextW * nextH * bytesPerPixel);

    stbir_resize(prevPixels.data(),
                 int(prevWidth),
                 int(prevHeight),
                 0,
                 nextPixels.data(),
                 int(nextW),
                 int(nextH),
                 0,
                 stbir_pixel_layout(channels),
                 dataType,
                 STBIR_EDGE_CLAMP,
                 STBIR_FILTER_DEFAULT);

    prevPixels = std::move(nextPixels);
    prevWidth  = nextW;
    prevHeight = nextH;
  }

  image->pixels    = std::move(allPixels);
  image->subImages = std::move(newSubImages);
  image->mipLevels = image->subImages.size();

  LOG_DEBUG("Generated " + std::to_string(image->mipLevels) + " mip levels via stb_image_resize2");
}

}  // namespace arise