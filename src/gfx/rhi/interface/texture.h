#ifndef ARISE_TEXTURE_H
#define ARISE_TEXTURE_H

#include "gfx/rhi/common/rhi_enums.h"
#include "gfx/rhi/common/rhi_types.h"

namespace arise {
namespace gfx {
namespace rhi {

/**
 * Represents a texture resource on the GPU
 *
 * This class provides a common interface for texture operations across rendering backends.
 * It abstracts away the differences between Vulkan, DirectX12, and potentially other APIs.
 */
class Texture {
  public:
  Texture(const TextureDesc& desc)
      : m_desc_(desc) {}

  virtual ~Texture() = default;

  const TextureDesc& getDesc() const { return m_desc_; }

  TextureFormat getFormat() const { return m_desc_.format; }

  uint32_t getWidth() const { return m_desc_.width; }

  uint32_t getHeight() const { return m_desc_.height; }

  uint32_t getDepth() const { return m_desc_.depth; }

  uint32_t getMipLevels() const { return m_desc_.mipLevels; }

  uint32_t getArraySize() const { return m_desc_.arraySize; }

  TextureType getType() const { return m_desc_.type; }

  // Get texture creation flags
  bool hasRtvUsage() const { return (m_desc_.createFlags & TextureCreateFlag::Rtv) != TextureCreateFlag::None; }

  bool hasUavUsage() const { return (m_desc_.createFlags & TextureCreateFlag::Uav) != TextureCreateFlag::None; }

  bool hasDsvUsage() const { return (m_desc_.createFlags & TextureCreateFlag::Dsv) != TextureCreateFlag::None; }

  virtual ResourceLayout getCurrentLayoutType() const { return m_currentLayout_; }

  protected:
  TextureDesc    m_desc_;
  ResourceLayout m_currentLayout_;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_TEXTURE_H