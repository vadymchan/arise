#include "utils/third_party/directx_tex_util.h"

#include "gfx/rhi/backends/dx12/rhi_enums_dx12.h"
#include "utils/logger/global_logger.h"

#include <algorithm>

namespace arise {

using gfx::rhi::TextureType;

const std::unordered_set<std::string> DirectXTexImageLoader::supportedExtensions_ = {".dds"};

bool DirectXTexImageLoader::supportsFormat(const std::string& extension) const {
  return supportedExtensions_.contains(extension);
}

TextureType DirectXTexImageLoader::determineDimension_(const DirectX::TexMetadata& metadata) const {
  TextureType baseDimension;
  switch (metadata.dimension) {
    case DirectX::TEX_DIMENSION_TEXTURE1D:
      baseDimension = (metadata.arraySize > 1) ? TextureType::Texture1DArray : TextureType::Texture1D;
      break;
    case DirectX::TEX_DIMENSION_TEXTURE2D:
      baseDimension = (metadata.IsCubemap())   ? TextureType::TextureCube
                    : (metadata.arraySize > 1) ? TextureType::Texture2DArray
                                               : TextureType::Texture2D;
      break;
    case DirectX::TEX_DIMENSION_TEXTURE3D:
      baseDimension = (metadata.arraySize > 1) ? TextureType::Texture3DArray : TextureType::Texture3D;
      break;
    default:
      GlobalLogger::Log(LogLevel::Warning,
                        "Unsupported texture dimension in DirectXTexImageLoader. "
                        "Defaulting to TextureType::Count.");
      baseDimension = TextureType::Count;
      break;
  }
  return baseDimension;
}

std::unique_ptr<Image> DirectXTexImageLoader::loadImage(const std::filesystem::path& filepath) {
  DirectX::TexMetadata  metadata;
  DirectX::ScratchImage scratchImage;

  HRESULT hr = DirectX::LoadFromDDSFile(filepath.wstring().c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage);

  if (FAILED(hr)) {
    GlobalLogger::Log(LogLevel::Error,
                      "Failed to load DDS image: " + filepath.string() + " [HRESULT: " + std::to_string(hr) + "]");
    return nullptr;
  }

  size_t totalBytes = scratchImage.GetPixelsSize();

  std::vector<std::byte> pixels(reinterpret_cast<std::byte*>(scratchImage.GetPixels()),
                                reinterpret_cast<std::byte*>(scratchImage.GetPixels()) + totalBytes);

  auto image       = std::make_unique<Image>();
  image->width     = metadata.width;
  image->height    = metadata.height;
  image->depth     = metadata.depth;
  image->mipLevels = metadata.mipLevels;
  image->arraySize = metadata.arraySize;
  image->format    = gfx::rhi::g_getTextureFormatDx12(metadata.format);
  image->dimension = determineDimension_(metadata);
  image->pixels    = std::move(pixels);

  image->subImages.reserve(image->mipLevels * image->arraySize * image->depth);

  for (auto mip = 0; mip < image->mipLevels; ++mip) {
    for (auto arraySlice = 0; arraySlice < image->arraySize; ++arraySlice) {
      for (auto slice = 0; slice < image->depth; ++slice) {
        const auto* img = scratchImage.GetImage(mip, arraySlice, slice);
        if (!img) {
          GlobalLogger::Log(LogLevel::Warning,
                            "Failed to retrieve image data for mip level " + std::to_string(mip) + ", array slice "
                                + std::to_string(arraySlice) + ", depth slice " + std::to_string(slice) + ".");
          continue;
        }

        SubImage subImage;
        subImage.width       = img->width;
        subImage.height      = img->height;
        subImage.rowPitch    = img->rowPitch;
        subImage.slicePitch  = img->slicePitch;
        subImage.pixelOffset = reinterpret_cast<const std::byte*>(img->pixels)
                             - reinterpret_cast<const std::byte*>(scratchImage.GetPixels());

        image->subImages.emplace_back(subImage);
      }
    }
  }

  return image;
}

}  // namespace arise