#ifdef ARISE_USE_LIBKTX

#include "utils/third_party/ktx_image_loader.h"

#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"
#include "utils/logger/log.h"

namespace arise {

using gfx::rhi::TextureFormat;
using gfx::rhi::TextureType;

const std::unordered_set<std::string> KtxImageLoader::supportedExtensions_ = {".ktx", ".ktx2"};

bool KtxImageLoader::supportsFormat(const std::string& extension) const {
  return supportedExtensions_.contains(extension);
}

std::unique_ptr<Image> KtxImageLoader::loadImage(const std::filesystem::path& filepath) {
  auto extension = filepath.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  if (extension == ".ktx2") {
    return loadKtx2_(filepath);
  } else {
    return loadKtx1_(filepath);
  }
}

std::unique_ptr<Image> KtxImageLoader::loadKtx2_(const std::filesystem::path& filepath) {
  ktxTexture2*                texture = nullptr;
  const ktxTextureCreateFlags flags = KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT | KTX_TEXTURE_CREATE_CHECK_GLTF_BASISU_BIT;

  ktxResult res = ktxTexture2_CreateFromNamedFile(filepath.string().c_str(), flags, &texture);
  if (res != KTX_SUCCESS) {
    LOG_ERROR("Failed to load KTX2 '{}': {}", filepath.string(), ktxErrorString(res));
    return nullptr;
  }
  if (!texture) {
    LOG_ERROR("KTX2 loader returned empty texture for '{}'", filepath.string());
    return nullptr;
  }

  bool          needsTranscode = (ktxTexture2_NeedsTranscoding(texture) == KTX_TRUE);
  TextureFormat textureFormat{};

  if (needsTranscode) {
    uint32_t numComponents = ktxTexture2_GetNumComponents(texture);
    bool     hasAlpha      = (numComponents == 4);
    textureFormat          = determineBestFormat_(numComponents, hasAlpha);

    ktxResult txRes = ktxTexture2_TranscodeBasis(texture, transcodeFormat_(textureFormat), 0);
    if (txRes != KTX_SUCCESS) {
      LOG_ERROR("Failed to transcode BasisU for '{}': {}", filepath.string(), ktxErrorString(txRes));
      ktxTexture_Destroy(reinterpret_cast<ktxTexture*>(texture));
      return nullptr;
    }
  } else {
    textureFormat = gfx::rhi::g_getTextureFormatVk(static_cast<VkFormat>(texture->vkFormat));
  }

  auto image = copyToImageStruct_(reinterpret_cast<ktxTexture*>(texture), textureFormat);
  ktxTexture_Destroy(reinterpret_cast<ktxTexture*>(texture));
  return image;
}

std::unique_ptr<Image> KtxImageLoader::loadKtx1_(const std::filesystem::path& filepath) {
  ktxTexture1*                texture = nullptr;
  const ktxTextureCreateFlags flags   = KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT;

  ktxResult result = ktxTexture1_CreateFromNamedFile(filepath.string().c_str(), flags, &texture);
  if (result != KTX_SUCCESS) {
    LOG_ERROR("Failed to load KTX1 '{}': {}", filepath.string(), ktxErrorString(result));
    return nullptr;
  }
  if (!texture) {
    LOG_ERROR("KTX1 loader returned empty texture for '{}'", filepath.string());
    return nullptr;
  }

  // TODO: use fallback since KTX1 doesn't have vkFormat (only GL format)
  TextureFormat format = TextureFormat::Rgba8;
  LOG_WARN("Using RGBA8 fallback for KTX1 '{}'", filepath.string());

  auto image = copyToImageStruct_(reinterpret_cast<ktxTexture*>(texture), format);
  ktxTexture_Destroy(reinterpret_cast<ktxTexture*>(texture));
  return image;
}

std::unique_ptr<Image> KtxImageLoader::copyToImageStruct_(ktxTexture* texture, TextureFormat format) {
  const uint8_t* data = ktxTexture_GetData(texture);
  size_t         size = ktxTexture_GetDataSize(texture);

  auto image       = std::make_unique<Image>();
  image->width     = texture->baseWidth;
  image->height    = texture->baseHeight;
  image->depth     = texture->baseDepth ? texture->baseDepth : 1;
  image->mipLevels = texture->numLevels;
  image->arraySize = texture->numLayers ? texture->numLayers : 1;
  image->dimension = determineDimension_(texture);
  image->format    = format;

  image->pixels.resize(size);
  std::memcpy(image->pixels.data(), data, size);

  image->subImages.reserve(image->mipLevels * image->arraySize);
  size_t offset = 0;
  for (uint32_t level = 0; level < texture->numLevels; ++level) {
    for (uint32_t layer = 0; layer < image->arraySize; ++layer) {
      if (ktxTexture_GetImageOffset(texture, level, layer, 0, &offset) != KTX_SUCCESS) {
        LOG_WARN("Cannot get image offset at level {}, layer {}", level, layer);
        continue;
      }
      SubImage subImage;
      subImage.width       = std::max<size_t>(1, texture->baseWidth >> level);
      subImage.height      = std::max<size_t>(1, texture->baseHeight >> level);
      subImage.rowPitch    = 0;
      subImage.slicePitch  = ktxTexture_GetImageSize(texture, level);
      subImage.pixelOffset = offset;
      image->subImages.emplace_back(subImage);
    }
  }
  return image;
}

TextureFormat KtxImageLoader::determineBestFormat_(uint32_t components, bool hasAlpha) const {
  if (components <= 2) {
    if (!hasAlpha && m_deviceCaps_.bc4) {
      return TextureFormat::Bc4Unorm;
    }
    if (hasAlpha && m_deviceCaps_.bc5) {
      return TextureFormat::Bc5Unorm;
    }
    return hasAlpha ? TextureFormat::Rg8 : TextureFormat::R8;
  }
  if (!hasAlpha) {
    if (m_deviceCaps_.bc7) {
      return TextureFormat::Bc7Unorm;
    }
    if (m_deviceCaps_.bc1) {
      return TextureFormat::Bc1Unorm;
    }
    if (m_deviceCaps_.bc3) {
      return TextureFormat::Bc3Unorm;
    }
    return TextureFormat::Rgb8;
  }
  if (m_deviceCaps_.bc7) {
    return TextureFormat::Bc7Unorm;
  }
  if (m_deviceCaps_.bc3) {
    return TextureFormat::Bc3Unorm;
  }
  return TextureFormat::Rgba8;
}

ktx_transcode_fmt_e KtxImageLoader::transcodeFormat_(TextureFormat format) {
  switch (format) {
    case TextureFormat::Bc1Unorm:
      return KTX_TTF_BC1_RGB;
    case TextureFormat::Bc3Unorm:
      return KTX_TTF_BC3_RGBA;
    case TextureFormat::Bc4Unorm:
      return KTX_TTF_BC4_R;
    case TextureFormat::Bc5Unorm:
      return KTX_TTF_BC5_RG;
    case TextureFormat::Bc7Unorm:
      return KTX_TTF_BC7_RGBA;
    default:
      return KTX_TTF_RGBA32;
  }
}

gfx::rhi::TextureType KtxImageLoader::determineDimension_(ktxTexture* texture) {
  if (texture->numDimensions == 1) {
    return texture->numLevels > 1 ? TextureType::Texture1DArray : TextureType::Texture1D;
  } else if (texture->numDimensions == 2) {
    if (texture->isCubemap) {
      return TextureType::TextureCube;
    }
    return texture->numLayers > 1 ? TextureType::Texture2DArray : TextureType::Texture2D;
  } else if (texture->numDimensions == 3) {
    return TextureType::Texture3D;
  }
  return TextureType::Count;
}

}  // namespace arise
#endif  // ARISE_USE_LIBKTX
