// TODO: rename to ktx_utils.h

#ifndef ARISE_KTX_UTILS
#define ARISE_KTX_UTILS

#ifdef ARISE_USE_LIBKTX

#include "file_loader/image_file_loader.h"
#include "gfx/rhi/common/rhi_enums.h"

#include <ktx.h>

#include <filesystem>
#include <unordered_set>

namespace arise {

class KtxImageLoader final : public IImageLoader {
  public:
  KtxImageLoader()           = default;
  ~KtxImageLoader() override = default;

  std::unique_ptr<Image> loadImage(const std::filesystem::path& filepath) override;
  bool                   supportsFormat(const std::string& extension) const override;

  private:
  // TODO: consider changing to real device capabilities (taken from gfx::rhi::Device)
  struct DeviceCapabilities {
    bool bc1  = true;
    bool bc3  = true;
    bool bc4  = true;
    bool bc5  = true;
    bool bc6h = true;
    bool bc7  = true;
    bool astc = false;
    bool etc2 = false;
  } m_deviceCaps_;

  gfx::rhi::TextureFormat determineBestFormat_(uint32_t components, bool hasAlpha) const;

  static ktx_transcode_fmt_e   transcodeFormat_(gfx::rhi::TextureFormat format);
  static gfx::rhi::TextureType determineDimension_(ktxTexture* texture);

  std::unique_ptr<Image> loadKtx2_(const std::filesystem::path& filepath);
  std::unique_ptr<Image> loadKtx1_(const std::filesystem::path& filepath);

  std::unique_ptr<Image> copyToImageStruct_(ktxTexture* texture, gfx::rhi::TextureFormat format);

  static const std::unordered_set<std::string> supportedExtensions_;
};

}  // namespace arise
#endif  // USE_LIBKTX

#endif  // ARISE_KTX_UTILS