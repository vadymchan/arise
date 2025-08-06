#include "gfx/rhi/backends/vulkan/sampler_vk.h"

#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"
#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

SamplerVk::SamplerVk(const SamplerDesc& desc, DeviceVk* device)
    : Sampler(desc)
    , m_device_(device) {
  VkSamplerCreateInfo samplerInfo = {};
  samplerInfo.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

  samplerInfo.magFilter = g_getTextureFilterTypeVk(desc.magFilter);
  samplerInfo.minFilter = g_getTextureFilterTypeVk(desc.minFilter);

  samplerInfo.addressModeU = g_getTextureAddressModeVk(desc.addressModeU);
  samplerInfo.addressModeV = g_getTextureAddressModeVk(desc.addressModeV);
  samplerInfo.addressModeW = g_getTextureAddressModeVk(desc.addressModeW);

  samplerInfo.mipmapMode = g_getTextureMipmapModeVk(desc.minFilter);

  samplerInfo.mipLodBias = desc.mipLodBias;
  samplerInfo.minLod     = desc.minLod;
  samplerInfo.maxLod     = desc.maxLod;

  samplerInfo.anisotropyEnable = desc.anisotropyEnable ? VK_TRUE : VK_FALSE;
  samplerInfo.maxAnisotropy    = desc.maxAnisotropy;

  samplerInfo.compareEnable = desc.compareEnable ? VK_TRUE : VK_FALSE;
  samplerInfo.compareOp     = g_getCompareOpVk(desc.compareOp);

  // TODO: consider use VK_EXT_custom_border_color if available
  if (desc.borderColor[0] == 0.0f && desc.borderColor[1] == 0.0f && desc.borderColor[2] == 0.0f
      && desc.borderColor[3] == 0.0f) {
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  } else if (desc.borderColor[0] == 0.0f && desc.borderColor[1] == 0.0f && desc.borderColor[2] == 0.0f
             && desc.borderColor[3] == 1.0f) {
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
  } else if (desc.borderColor[0] == 1.0f && desc.borderColor[1] == 1.0f && desc.borderColor[2] == 1.0f
             && desc.borderColor[3] == 1.0f) {
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  } else {
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    LOG_WARN("Custom border colors not fully supported in Vulkan sampler");
  }

  samplerInfo.unnormalizedCoordinates = VK_FALSE;

  if (vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &m_sampler_) != VK_SUCCESS) {
    LOG_ERROR("Failed to create Vulkan sampler");
  }
}

SamplerVk::~SamplerVk() {
  if (m_device_ && m_sampler_ != VK_NULL_HANDLE) {
    vkDestroySampler(m_device_->getDevice(), m_sampler_, nullptr);
    m_sampler_ = VK_NULL_HANDLE;
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise