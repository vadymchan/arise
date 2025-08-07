#include "utils/texture/texture_manager.h"

#include "gfx/rhi/interface/command_buffer.h"
#include "gfx/rhi/interface/synchronization.h"
#include "utils/image/image_manager.h"
#include "utils/logger/log.h"
#include "utils/resource/resource_deletion_manager.h"
#include "utils/service/service_locator.h"

namespace arise {

TextureManager::TextureManager(gfx::rhi::Device* device)
    : m_device(device)
    , m_textureCounter(0) {
  if (!m_device) {
    LOG_ERROR("Device is null");
  }
}

TextureManager::~TextureManager() {
  if (!m_textures.empty()) {
    LOG_INFO("TextureManager destroyed, releasing {} textures", m_textures.size());

    for (const auto& [name, texture] : m_textures) {
      LOG_INFO("Released texture: {}", name);
    }
  }
  release();
}

gfx::rhi::Texture* TextureManager::createTexture(Image* image, const std::string& name) {
  if (!m_device) {
    LOG_ERROR("Cannot create texture, device is null");
    return nullptr;
  }

  if (!image) {
    LOG_ERROR("Cannot create texture, image is null");
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
    LOG_WARN("Texture with name '{}' already exists, will be replaced", textureName);
    m_textures.erase(textureName);
  }

  auto texture = m_device->createTexture(desc);
  if (!texture) {
    LOG_ERROR("Failed to create texture '{}'", textureName);
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

  LOG_INFO("Created texture '{}' with dimensions {}x{}", textureName, desc.width, desc.height);

  return texturePtr;
}

gfx::rhi::Texture* TextureManager::createTextureFromFile(const std::filesystem::path& filepath,
                                                         const std::string&           name) {
  auto imageManager = ServiceLocator::s_get<ImageManager>();
  if (!imageManager) {
    LOG_ERROR("Cannot create texture from file, ImageManager not available");
    return nullptr;
  }

  Image* image = imageManager->getImage(filepath);
  if (!image) {
    LOG_ERROR("Failed to load image from file: {}", filepath.string());
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
    LOG_ERROR("Cannot create render target, device is null");
    return nullptr;
  }

  if (width == 0 || height == 0) {
    LOG_ERROR("Invalid render target dimensions");
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
    LOG_WARN("Texture with name '{}' already exists, will be replaced", textureName);
    m_textures.erase(textureName);
  }

  auto texture = m_device->createTexture(desc);
  if (!texture) {
    LOG_ERROR("Failed to create render target '{}'", textureName);
    return nullptr;
  }

  gfx::rhi::Texture* texturePtr = texture.get();
  m_textures[textureName]       = std::move(texture);

  LOG_INFO("Created render target '{}' with dimensions {}x{}", textureName, width, height);

  return texturePtr;
}

gfx::rhi::Texture* TextureManager::createDepthStencil(uint32_t                width,
                                                      uint32_t                height,
                                                      gfx::rhi::TextureFormat format,
                                                      const std::string&      name) {
  if (!m_device) {
    LOG_ERROR("Cannot create depth stencil, device is null");
    return nullptr;
  }

  if (width == 0 || height == 0) {
    LOG_ERROR("Invalid depth stencil dimensions");
    return nullptr;
  }

  if (!gfx::rhi::g_isDepthFormat(format)) {
    LOG_ERROR("Invalid format for depth stencil, must be a depth format");
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
    LOG_WARN("Texture with name '{}' already exists, will be replaced", textureName);
    m_textures.erase(textureName);
  }

  auto texture = m_device->createTexture(desc);
  if (!texture) {
    LOG_ERROR("Failed to create depth stencil '{}'", textureName);
    return nullptr;
  }

  gfx::rhi::Texture* texturePtr = texture.get();
  m_textures[textureName]       = std::move(texture);

  LOG_INFO("Created depth stencil '{}' with dimensions {}x{}", textureName, width, height);

  return texturePtr;
}

gfx::rhi::Texture* TextureManager::addTexture(std::unique_ptr<gfx::rhi::Texture> texture, const std::string& name) {
  if (!texture) {
    LOG_ERROR("Cannot add null texture");
    return nullptr;
  }

  std::string textureName = name.empty() ? generateUniqueName_("Texture") : name;

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_textures.contains(textureName)) {
    LOG_WARN("Texture with name '{}' already exists, will be replaced", textureName);
    m_textures.erase(textureName);
  }

  gfx::rhi::Texture* texturePtr = texture.get();
  m_textures[textureName]       = std::move(texture);

  LOG_INFO("Added external texture '{}'", textureName);

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
            LOG_INFO("Texture '{}' deleted", name);
            m_textures.erase(name);
          },
          name,
          "Texture");

      return true;
    } else {
      LOG_INFO("Removing texture '{}'", name);
      m_textures.erase(it);
      return true;
    }
  }

  LOG_WARN("Attempted to remove non-existent texture '{}'", name);
  return false;
}

bool TextureManager::removeTexture(gfx::rhi::Texture* texture) {
  if (!texture) {
    LOG_ERROR("Cannot remove null texture");
    return false;
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  auto                        it = findTexture_(texture);
  if (it != m_textures.end()) {
    std::string textureName = it->first;

    auto deletionManager = ServiceLocator::s_get<ResourceDeletionManager>();
    if (deletionManager) {
      deletionManager->enqueueForDeletion<gfx::rhi::Texture>(
          texture,
          [this, textureName](gfx::rhi::Texture*) {
            std::lock_guard<std::mutex> lock(m_mutex);
            LOG_INFO("Texture '{}' deleted", textureName);
            m_textures.erase(textureName);
          },
          textureName,
          "Texture");
      return true;
    } else {
      LOG_INFO("Removing texture '{}'", textureName);
      m_textures.erase(it);
      return true;
    }
  }

  LOG_WARN("Texture not found in manager");
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

  LOG_INFO("Releasing {} textures", m_textures.size());

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

}  // namespace arise