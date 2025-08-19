#ifndef ARISE_RHI_ENUMS_VK_H
#define ARISE_RHI_ENUMS_VK_H

#include "gfx/rhi/common/rhi_enums.h"

#include <vulkan/vulkan.h>

#include <unordered_map>

namespace arise {
namespace gfx {
namespace rhi {

// if you need reverse mapping (from vk to rhi) add it here

VkFilter g_getTextureFilterTypeVk(TextureFilter textureFilter);

VkSamplerAddressMode g_getTextureAddressModeVk(TextureAddressMode addressMode);

VkSamplerMipmapMode g_getTextureMipmapModeVk(TextureFilter textureFilter);

VkFormat      g_getTextureFormatVk(TextureFormat textureFormat);
TextureFormat g_getTextureFormatVk(VkFormat format);

VkFormat g_getVertexFormatVk(VertexFormat vertexFormat);

int g_getTextureComponentCountVk(TextureFormat type);
int g_getTexturePixelSizeVk(TextureFormat type);

void g_getAttachmentLoadStoreOpVk(VkAttachmentLoadOp& loadOp, VkAttachmentStoreOp& storeOp, AttachmentLoadStoreOp type);

VkShaderStageFlags g_getShaderStageFlagsVk(ShaderStageFlag type);

VkShaderStageFlagBits g_getShaderStageBitsVk(ShaderStageFlag type);

VkPrimitiveTopology g_getPrimitiveTopologyVk(PrimitiveType primitiveType);

VkVertexInputRate g_getVertexInputRateVk(VertexInputRate inputRate);

VkPolygonMode g_getPolygonModeVk(PolygonMode polygonMode);

VkCullModeFlags g_getCullModeVk(CullMode cullMode);

VkFrontFace g_getFrontFaceVk(FrontFace frontFace);

VkStencilOp g_getStencilOpVk(StencilOp stencilOp);

VkCompareOp g_getCompareOpVk(CompareOp compareOp);

VkBlendFactor g_getBlendFactorVk(BlendFactor blendFactor);

VkBlendOp g_getBlendOpVk(BlendOp blendOp);

VkLogicOp g_getLogicOpVk(LogicOp logicOp);

VkImageLayout  g_getImageLayoutVk(ResourceLayout resourceLayout);
ResourceLayout g_getImageLayoutVk(VkImageLayout resourceLayout);

VkColorComponentFlags g_getColorMaskVk(ColorMask type);

VkDescriptorType g_getShaderBindingTypeVk(ShaderBindingType type);

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_RHI_ENUMS_VK_H