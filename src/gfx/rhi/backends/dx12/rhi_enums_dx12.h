#ifndef ARISE_RHI_DX12_UTIL_H
#define ARISE_RHI_DX12_UTIL_H

#include "gfx/rhi/common/rhi_enums.h"
#include "platform/windows/windows_platform_setup.h"

#ifdef ARISE_USE_DX12

#include <unordered_map>

namespace arise {
namespace gfx {
namespace rhi {

// if you need reverse mapping (from dx12 to rhi) add it here

DXGI_FORMAT   g_getTextureFormatDx12(TextureFormat textureFormat);
TextureFormat g_getTextureFormatDx12(DXGI_FORMAT formatType);

int g_getTextureComponentCountDx12(TextureFormat type);
int g_getTexturePixelSizeDx12(TextureFormat type);

TextureType              g_getTextureDimensionDx12(D3D12_RESOURCE_DIMENSION type, bool isArray);
D3D12_RESOURCE_DIMENSION g_getTextureDimensionDx12(TextureType type);

D3D12_DESCRIPTOR_RANGE_TYPE g_getShaderBindingTypeDx12(ShaderBindingType bindingType);

D3D12_TEXTURE_ADDRESS_MODE g_getTextureAddressModeDx12(TextureAddressMode addressMode);

D3D12_COMPARISON_FUNC g_getCompareOpDx12(CompareOp compareOp);

D3D12_STENCIL_OP g_getStencilOpDx12(StencilOp stencilOp);

D3D12_PRIMITIVE_TOPOLOGY_TYPE g_getPrimitiveTopologyTypeOnlyDx12(PrimitiveType type);

D3D_PRIMITIVE_TOPOLOGY g_getPrimitiveTopologyDx12(PrimitiveType type);

D3D12_INPUT_CLASSIFICATION g_getVertexInputRateDx12(VertexInputRate inputRate);

D3D12_FILL_MODE g_getPolygonModeDx12(PolygonMode polygonMode);

D3D12_CULL_MODE g_getCullModeDx12(CullMode cullMode);

D3D12_BLEND g_getBlendFactorDx12(BlendFactor type);

D3D12_BLEND_OP g_getBlendOpDx12(BlendOp blendOp);

D3D12_LOGIC_OP g_getLogicOpDx12(LogicOp logicOp);

uint8_t g_getColorMaskDx12(ColorMask type);

D3D12_FILTER g_getTextureFilterDx12(TextureFilter minification, TextureFilter magnification, bool isComparison = false);

D3D12_SHADER_VISIBILITY g_getShaderVisibilityDx12(ShaderStageFlag stageFlags);

void g_getDepthFormatForSRV(DXGI_FORMAT& texFormat, DXGI_FORMAT& srvFormat, DXGI_FORMAT originalTexFormat);

D3D12_RESOURCE_STATES g_getResourceLayoutDx12(ResourceLayout resourceLayout);
ResourceLayout        g_getResourceLayoutDx12(D3D12_RESOURCE_STATES resourceState);

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_USE_DX12
#endif  // ARISE_RHI_DX12_UTIL_H