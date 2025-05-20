#include "utils/texture/texture_manager.h"

#include "gfx/rhi/interface/command_buffer.h"
#include "gfx/rhi/interface/synchronization.h"
#include "utils/image/image_manager.h"
#include "utils/resource/resource_deletion_manager.h"
#include "utils/service/service_locator.h"

namespace game_engine {

TextureManager::TextureManager(gfx::rhi::Device* device)
    : m_device(device)
    , m_textureCounter(0) {
  if (!m_device) {
    GlobalLogger::Log(LogLevel::Error, "Device is null");
  }
}

TextureManager::~TextureManager() {
  release();
}

gfx::rhi::Texture* TextureManager::createTexture(Image* image, const std::string& name) {
  if (!m_device) {
    GlobalLogger::Log(LogLevel::Error, "Cannot create texture, device is null");
    return nullptr;
  }

  if (!image) {
    GlobalLogger::Log(LogLevel::Error, "Cannot create texture, image is null");
    return nullptr;
  }

  gfx::rhi::TextureDesc desc;
  desc.width       = static_cast<uint32_t>(image->width);
  desc.height      = static_cast<uint32_t>(image->height);
  desc.depth       = static_cast<uint32_t>(image->depth);
  desc.format      = image->format;
  desc.type        = image->dimension;
  desc.mipLevels   = static_cast<uint32_t>(image->mipLevels);
  desc.arraySize   = static_cast<uint32_t>(image->arraySize);
  desc.createFlags = gfx::rhi::TextureCreateFlag::TransferDst;
  desc.debugName   = name.empty() ? "unnamed_loaded_texture" : name.c_str();

  std::string textureName = name.empty() ? generateUniqueName_("Texture") : name;

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_textures.contains(textureName)) {
    GlobalLogger::Log(LogLevel::Warning, "Texture with name '" + textureName + "' already exists, will be replaced");
    m_textures.erase(textureName);
  }

  auto texture = m_device->createTexture(desc);
  if (!texture) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create texture '" + textureName + "'");
    return nullptr;
  }

  if (!image->pixels.empty() && !image->subImages.empty()) {
    if (desc.mipLevels > 1 || desc.arraySize > 1) {
      for (uint32_t arraySlice = 0; arraySlice < desc.arraySize; ++arraySlice) {
        for (uint32_t mipLevel = 0; mipLevel < desc.mipLevels; ++mipLevel) {
          size_t subImageIndex = mipLevel + arraySlice * desc.mipLevels;

          if (subImageIndex < image->subImages.size()) {
            const auto& subImage = image->subImages[subImageIndex];

            const void* pixelData = image->pixels.data() + subImage.pixelOffset;
            size_t      pixelSize = subImage.slicePitch;

            m_device->updateTexture(texture.get(), pixelData, pixelSize, mipLevel, arraySlice);
          }
        }
      }
    } else {
      // Simple case: single mip level and array slice
      m_device->updateTexture(texture.get(), image->pixels.data(), image->pixels.size());
    }
  }

  if (texture) {
    gfx::rhi::ResourceBarrierDesc barrier;
    barrier.texture   = texture.get();
    barrier.oldLayout = texture->getCurrentLayoutType();
    barrier.newLayout = gfx::rhi::ResourceLayout::ShaderReadOnly;

    auto cmdBuffer = m_device->createCommandBuffer();
    cmdBuffer->reset();
    cmdBuffer->begin();
    cmdBuffer->resourceBarrier(barrier);
    cmdBuffer->end();

    auto fence = m_device->createFence();
    m_device->submitCommandBuffer(cmdBuffer.get(), fence.get());
    fence->wait();
  }

  gfx::rhi::Texture* texturePtr = texture.get();
  m_textures[textureName]       = std::move(texture);

  GlobalLogger::Log(LogLevel::Info,
                    "Created texture '" + textureName + "' with dimensions " + std::to_string(desc.width) + "x"
                        + std::to_string(desc.height));

  return texturePtr;
}

gfx::rhi::Texture* TextureManager::createTextureFromFile(const std::filesystem::path& filepath,
                                                         const std::string&           name) {
  auto imageManager = ServiceLocator::s_get<ImageManager>();
  if (!imageManager) {
    GlobalLogger::Log(LogLevel::Error, "Cannot create texture from file, ImageManager not available");
    return nullptr;
  }

  Image* image = imageManager->getImage(filepath);
  if (!image) {
    GlobalLogger::Log(LogLevel::Error, "Failed to load image from file: " + filepath.string());
    return nullptr;
  }

  std::string textureName = name.empty() ? filepath.filename().string() : name;

  return createTexture(image, textureName);
}

gfx::rhi::Texture* TextureManager::createRenderTarget(uint32_t                width,
                                                      uint32_t                height,
                                                      gfx::rhi::TextureFormat format,
                                                      const std::string&      name) {
  if (!m_device) {
    GlobalLogger::Log(LogLevel::Error, "Cannot create render target, device is null");
    return nullptr;
  }

  if (width == 0 || height == 0) {
    GlobalLogger::Log(LogLevel::Error, "Invalid render target dimensions");
    return nullptr;
  }

  gfx::rhi::TextureDesc desc;
  desc.width       = width;
  desc.height      = height;
  desc.depth       = 1;
  desc.format      = format;
  desc.type        = gfx::rhi::TextureType::Texture2D;
  desc.mipLevels   = 1;
  desc.arraySize   = 1;
  desc.createFlags = gfx::rhi::TextureCreateFlag::Rtv;
  desc.debugName   = name.empty() ? "unnamed_render_target" : name.c_str();

  std::string textureName = name.empty() ? generateUniqueName_("RenderTarget") : name;

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_textures.contains(textureName)) {
    GlobalLogger::Log(LogLevel::Warning, "Texture with name '" + textureName + "' already exists, will be replaced");
    m_textures.erase(textureName);
  }

  auto texture = m_device->createTexture(desc);
  if (!texture) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create render target '" + textureName + "'");
    return nullptr;
  }

  gfx::rhi::Texture* texturePtr = texture.get();
  m_textures[textureName]       = std::move(texture);

  GlobalLogger::Log(LogLevel::Info,
                    "Created render target '" + textureName + "' with dimensions " + std::to_string(width) + "x"
                        + std::to_string(height));

  return texturePtr;
}

gfx::rhi::Texture* TextureManager::createDepthStencil(uint32_t                width,
                                                      uint32_t                height,
                                                      gfx::rhi::TextureFormat format,
                                                      const std::string&      name) {
  if (!m_device) {
    GlobalLogger::Log(LogLevel::Error, "Cannot create depth stencil, device is null");
    return nullptr;
  }

  if (width == 0 || height == 0) {
    GlobalLogger::Log(LogLevel::Error, "Invalid depth stencil dimensions");
    return nullptr;
  }

  if (!gfx::rhi::g_isDepthFormat(format)) {
    GlobalLogger::Log(LogLevel::Error, "Invalid format for depth stencil, must be a depth format");
    return nullptr;
  }

  gfx::rhi::TextureDesc desc;
  desc.width       = width;
  desc.height      = height;
  desc.depth       = 1;
  desc.format      = format;
  desc.type        = gfx::rhi::TextureType::Texture2D;
  desc.mipLevels   = 1;
  desc.arraySize   = 1;
  desc.createFlags = gfx::rhi::TextureCreateFlag::Dsv;
  desc.debugName   = name.empty() ? "unnamed_depth_stencil" : name.c_str();

  std::string textureName = name.empty() ? generateUniqueName_("DepthStencil") : name;

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_textures.contains(textureName)) {
    GlobalLogger::Log(LogLevel::Warning, "Texture with name '" + textureName + "' already exists, will be replaced");
    m_textures.erase(textureName);
  }

  auto texture = m_device->createTexture(desc);
  if (!texture) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create depth stencil '" + textureName + "'");
    return nullptr;
  }

  gfx::rhi::Texture* texturePtr = texture.get();
  m_textures[textureName]       = std::move(texture);

  GlobalLogger::Log(LogLevel::Info,
                    "Created depth stencil '" + textureName + "' with dimensions " + std::to_string(width) + "x"
                        + std::to_string(height));

  return texturePtr;
}

gfx::rhi::Texture* TextureManager::addTexture(std::unique_ptr<gfx::rhi::Texture> texture, const std::string& name) {
  if (!texture) {
    GlobalLogger::Log(LogLevel::Error, "Cannot add null texture");
    return nullptr;
  }

  std::string textureName = name.empty() ? generateUniqueName_("Texture") : name;

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_textures.contains(textureName)) {
    GlobalLogger::Log(LogLevel::Warning, "Texture with name '" + textureName + "' already exists, will be replaced");
    m_textures.erase(textureName);
  }

  gfx::rhi::Texture* texturePtr = texture.get();
  m_textures[textureName]       = std::move(texture);

  GlobalLogger::Log(LogLevel::Info, "Added external texture '" + textureName + "'");

  return texturePtr;
}

gfx::rhi::Texture* TextureManager::getTexture(const std::string& name) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_textures.find(name);
  if (it != m_textures.end()) {
    return it->second.get();
  }

  return nullptr;
}

bool TextureManager::removeTexture(const std::string& name) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_textures.find(name);
  if (it != m_textures.end()) {
    auto deletionManager = ServiceLocator::s_get<ResourceDeletionManager>();
    if (deletionManager) {
      gfx::rhi::Texture* texturePtr = it->second.get();

      deletionManager->enqueueForDeletion<gfx::rhi::Texture>(
          texturePtr,
          [this, name](gfx::rhi::Texture*) {
            std::lock_guard<std::mutex> lock(m_mutex);
            GlobalLogger::Log(LogLevel::Info, "Texture '" + name + "' deleted");
            m_textures.erase(name);
          },
          name,
          "Texture");

      return true;
    } else {
      GlobalLogger::Log(LogLevel::Info, "Removing texture '" + name + "'");
      m_textures.erase(it);
      return true;
    }
  }

  GlobalLogger::Log(LogLevel::Warning, "Attempted to remove non-existent texture '" + name + "'");
  return false;
}

bool TextureManager::hasTexture(const std::string& name) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_textures.contains(name);
}

size_t TextureManager::getTextureCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_textures.size();
}

void TextureManager::release() {
  std::lock_guard<std::mutex> lock(m_mutex);

  GlobalLogger::Log(LogLevel::Info, "Releasing " + std::to_string(m_textures.size()) + " textures");

  m_textures.clear();
}

std::string TextureManager::generateUniqueName_(const std::string& prefix) {
  return prefix + "_" + std::to_string(m_textureCounter++);
}

std::unordered_map<std::string, std::unique_ptr<gfx::rhi::Texture>>::iterator TextureManager::findTexture_(
    const std::string& name) {
  return m_textures.find(name);
}

std::unordered_map<std::string, std::unique_ptr<gfx::rhi::Texture>>::iterator TextureManager::findTexture_(
    const gfx::rhi::Texture* texture) {
  for (auto it = m_textures.begin(); it != m_textures.end(); ++it) {
    if (it->second.get() == texture) {
      return it;
    }
  }
  return m_textures.end();
}

}  // namespace game_engine