#ifndef ARISE_TEXTURE_MANAGER_H
#define ARISE_TEXTURE_MANAGER_H

#include "gfx/rhi/interface/device.h"
#include "gfx/rhi/interface/texture.h"
#include "resources/image.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace arise {

class TextureManager {
  public:
  TextureManager(gfx::rhi::Device* device);

  ~TextureManager();

  gfx::rhi::Texture* createTexture(Image* image, const std::string& name = "");
  gfx::rhi::Texture* createTextureFromFile(const std::filesystem::path& filepath, const std::string& name = "");
  gfx::rhi::Texture* createRenderTarget(uint32_t                width,
                                        uint32_t                height,
                                        gfx::rhi::TextureFormat format = gfx::rhi::TextureFormat::Rgba8,
                                        const std::string&      name   = "");
  gfx::rhi::Texture* createDepthStencil(uint32_t                width,
                                        uint32_t                height,
                                        gfx::rhi::TextureFormat format = gfx::rhi::TextureFormat::D24S8,
                                        const std::string&      name   = "");

  gfx::rhi::Texture* addTexture(std::unique_ptr<gfx::rhi::Texture> texture, const std::string& name);

  gfx::rhi::Texture* getTexture(const std::string& name) const;

  bool removeTexture(const std::string& name);
  bool removeTexture(gfx::rhi::Texture* texture);

  bool hasTexture(const std::string& name) const;

  size_t getTextureCount() const;

  void release();

  private:
  std::string generateUniqueName_(const std::string& prefix);

  // TODO: not used, consider remove
  std::unordered_map<std::string, std::unique_ptr<gfx::rhi::Texture>>::iterator findTexture_(const std::string& name);

  std::unordered_map<std::string, std::unique_ptr<gfx::rhi::Texture>>::iterator findTexture_(
      const gfx::rhi::Texture* texture);

  private:
  gfx::rhi::Device*                                                   m_device;
  mutable std::mutex                                                  m_mutex;
  std::unordered_map<std::string, std::unique_ptr<gfx::rhi::Texture>> m_textures;
  uint32_t m_textureCounter;  // Counter for generating unique names
};

}  // namespace arise

#endif  // ARISE_TEXTURE_MANAGER_H