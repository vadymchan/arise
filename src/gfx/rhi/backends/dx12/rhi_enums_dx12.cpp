#include "gfx/rhi/backends/dx12/rhi_enums_dx12.h"

#ifdef ARISE_RHI_DX12

#include <cassert>

namespace arise {
namespace gfx {
namespace rhi {

// clang-format off
static const std::unordered_map<TextureFormat, DXGI_FORMAT> textureFormatMappingToDXGI = {
  { TextureFormat::Rgb8,       DXGI_FORMAT_R8G8B8A8_UNORM     }, // not support rgb8 -> rgba8
  { TextureFormat::Rgb32f,     DXGI_FORMAT_R32G32B32_FLOAT    }, 
  { TextureFormat::Rgb16f,     DXGI_FORMAT_R16G16B16A16_FLOAT }, // not support rgb16 -> rgba16
  { TextureFormat::R11g11b10f, DXGI_FORMAT_R11G11B10_FLOAT    },
  { TextureFormat::Rgba8,      DXGI_FORMAT_R8G8B8A8_UNORM     },
  { TextureFormat::Rgba16f,    DXGI_FORMAT_R16G16B16A16_FLOAT },
  { TextureFormat::Rgba32f,    DXGI_FORMAT_R32G32B32A32_FLOAT },
  { TextureFormat::Rgba8si,    DXGI_FORMAT_R8G8B8A8_SINT      },
  { TextureFormat::Rgba8ui,    DXGI_FORMAT_R8G8B8A8_UINT      },
  { TextureFormat::Bgra8,      DXGI_FORMAT_B8G8R8A8_UNORM     },
  { TextureFormat::R8,         DXGI_FORMAT_R8_UNORM           },
  { TextureFormat::R16f,       DXGI_FORMAT_R16_FLOAT          },
  { TextureFormat::R32f,       DXGI_FORMAT_R32_FLOAT          },
  { TextureFormat::R8ui,       DXGI_FORMAT_R8_UINT            },
  { TextureFormat::R32ui,      DXGI_FORMAT_R32_UINT           },
  { TextureFormat::Rg8,        DXGI_FORMAT_R8G8_UNORM         },
  { TextureFormat::Rg16f,      DXGI_FORMAT_R16G16_FLOAT       },
  { TextureFormat::Rg32f,      DXGI_FORMAT_R32G32_FLOAT       },
  { TextureFormat::D16,        DXGI_FORMAT_D16_UNORM          },
  { TextureFormat::D16S8,      DXGI_FORMAT_D24_UNORM_S8_UINT  }, // not support d16_s8 -> d24_s8
  { TextureFormat::D24,        DXGI_FORMAT_D24_UNORM_S8_UINT  }, // not support d24    -> d24_s8
  { TextureFormat::D24S8,      DXGI_FORMAT_D24_UNORM_S8_UINT  },
  { TextureFormat::D32,        DXGI_FORMAT_D32_FLOAT          },
  { TextureFormat::D32S8,      DXGI_FORMAT_D32_FLOAT          }, // not support d32_s8 -> d32
  { TextureFormat::Bc1Unorm,   DXGI_FORMAT_BC1_UNORM          },
  { TextureFormat::Bc2Unorm,   DXGI_FORMAT_BC2_UNORM          },
  { TextureFormat::Bc3Unorm,   DXGI_FORMAT_BC3_UNORM          },
  { TextureFormat::Bc4Unorm,   DXGI_FORMAT_BC4_UNORM          },
  { TextureFormat::Bc4Snorm,   DXGI_FORMAT_BC4_SNORM          },
  { TextureFormat::Bc5Unorm,   DXGI_FORMAT_BC5_UNORM          },
  { TextureFormat::Bc5Snorm,   DXGI_FORMAT_BC5_SNORM          },
  { TextureFormat::Bc6hUf16,   DXGI_FORMAT_BC6H_UF16          },
  { TextureFormat::Bc6hSf16,   DXGI_FORMAT_BC6H_SF16          },
  { TextureFormat::Bc7Unorm,   DXGI_FORMAT_BC7_UNORM          }
};

static const std::unordered_map<ShaderBindingType, D3D12_DESCRIPTOR_RANGE_TYPE> bindingTypeMapping = {
  { ShaderBindingType::Uniformbuffer,            D3D12_DESCRIPTOR_RANGE_TYPE_CBV     },
  { ShaderBindingType::UniformbufferDynamic,     D3D12_DESCRIPTOR_RANGE_TYPE_CBV     },
  { ShaderBindingType::TextureSamplerSrv,        D3D12_DESCRIPTOR_RANGE_TYPE_SRV     },
  { ShaderBindingType::TextureSrv,               D3D12_DESCRIPTOR_RANGE_TYPE_SRV     },
  { ShaderBindingType::TextureUav,               D3D12_DESCRIPTOR_RANGE_TYPE_UAV     },
  { ShaderBindingType::TextureArraySrv,          D3D12_DESCRIPTOR_RANGE_TYPE_SRV     },
  { ShaderBindingType::Sampler,                  D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER },
  { ShaderBindingType::BufferSrv,                D3D12_DESCRIPTOR_RANGE_TYPE_SRV     },
  { ShaderBindingType::BufferUav,                D3D12_DESCRIPTOR_RANGE_TYPE_UAV     },
  { ShaderBindingType::BufferUavDynamic,         D3D12_DESCRIPTOR_RANGE_TYPE_UAV     },
  { ShaderBindingType::BufferTexelSrv,           D3D12_DESCRIPTOR_RANGE_TYPE_SRV     },
  { ShaderBindingType::BufferTexelUav,           D3D12_DESCRIPTOR_RANGE_TYPE_UAV     },
  { ShaderBindingType::AccelerationStructureSrv, D3D12_DESCRIPTOR_RANGE_TYPE_SRV     },
  { ShaderBindingType::SubpassInputAttachment,   (D3D12_DESCRIPTOR_RANGE_TYPE)-1     }
};

static const std::unordered_map<TextureAddressMode, D3D12_TEXTURE_ADDRESS_MODE> addressModeMapping = {
  { TextureAddressMode::Repeat,            D3D12_TEXTURE_ADDRESS_MODE_WRAP        },
  { TextureAddressMode::MirroredRepeat,    D3D12_TEXTURE_ADDRESS_MODE_MIRROR      },
  { TextureAddressMode::ClampToEdge,       D3D12_TEXTURE_ADDRESS_MODE_CLAMP       },
  { TextureAddressMode::ClampToBorder,     D3D12_TEXTURE_ADDRESS_MODE_BORDER      },
  { TextureAddressMode::MirrorClampToEdge, D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE }
};

static const std::unordered_map<CompareOp, D3D12_COMPARISON_FUNC> compareOpMapping = {
  { CompareOp::Never,        D3D12_COMPARISON_FUNC_NEVER         },
  { CompareOp::Less,         D3D12_COMPARISON_FUNC_LESS          },
  { CompareOp::Equal,        D3D12_COMPARISON_FUNC_EQUAL         },
  { CompareOp::LessEqual,    D3D12_COMPARISON_FUNC_LESS_EQUAL    },
  { CompareOp::Greater,      D3D12_COMPARISON_FUNC_GREATER       },
  { CompareOp::NotEqual,     D3D12_COMPARISON_FUNC_NOT_EQUAL     },
  { CompareOp::GreaterEqual, D3D12_COMPARISON_FUNC_GREATER_EQUAL },
  { CompareOp::Always,       D3D12_COMPARISON_FUNC_ALWAYS        }
};

static const std::unordered_map<StencilOp, D3D12_STENCIL_OP> stencilOpMapping = {
  { StencilOp::Keep,     D3D12_STENCIL_OP_KEEP     },
  { StencilOp::Zero,     D3D12_STENCIL_OP_ZERO     },
  { StencilOp::Replace,  D3D12_STENCIL_OP_REPLACE  },
  { StencilOp::Incr,     D3D12_STENCIL_OP_INCR_SAT },
  { StencilOp::IncrWrap, D3D12_STENCIL_OP_INCR     },
  { StencilOp::Decr,     D3D12_STENCIL_OP_DECR_SAT },
  { StencilOp::DecrWrap, D3D12_STENCIL_OP_DECR     },
  { StencilOp::Invert,   D3D12_STENCIL_OP_INVERT   }
};

static const std::unordered_map<PrimitiveType, D3D12_PRIMITIVE_TOPOLOGY_TYPE> primitiveTopologyTypeMapping = {
  { PrimitiveType::Points,                 D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT    },
  { PrimitiveType::Lines,                  D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE     }, // use this as a key in reverse map
  { PrimitiveType::LinesAdjacency,         D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE     },
  { PrimitiveType::LineStripAdjacency,     D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE     },
  { PrimitiveType::Triangles,              D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE }, // use this as a key in reverse map
  { PrimitiveType::TriangleStrip,          D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE },
  { PrimitiveType::TrianglesAdjacency,     D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE },
  { PrimitiveType::TriangleStripAdjacency, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE }
};

static const std::unordered_map<PrimitiveType, D3D_PRIMITIVE_TOPOLOGY> primitiveTopologyMapping = {
  { PrimitiveType::Points,                 D3D_PRIMITIVE_TOPOLOGY_POINTLIST         },
  { PrimitiveType::Lines,                  D3D_PRIMITIVE_TOPOLOGY_LINELIST          },
  { PrimitiveType::LinesAdjacency,         D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ      },
  { PrimitiveType::LineStripAdjacency,     D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ     },
  { PrimitiveType::Triangles,              D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST      },
  { PrimitiveType::TriangleStrip,          D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP     },
  { PrimitiveType::TrianglesAdjacency,     D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ  },
  { PrimitiveType::TriangleStripAdjacency, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ }
};

static const std::unordered_map<VertexInputRate, D3D12_INPUT_CLASSIFICATION> vertexInputRateMapping = {
  { VertexInputRate::Vertex,   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA   },
  { VertexInputRate::Instance, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA }
};

static const std::unordered_map<PolygonMode, D3D12_FILL_MODE> polygonModeMapping = {
  { PolygonMode::Point, D3D12_FILL_MODE_WIREFRAME }, // Not supported, fallback to wireframe
  { PolygonMode::Line,  D3D12_FILL_MODE_WIREFRAME }, // use this as a key in reverse map
  { PolygonMode::Fill,  D3D12_FILL_MODE_SOLID     }
};

static const std::unordered_map<CullMode, D3D12_CULL_MODE> cullModeMapping = {
  { CullMode::None,         D3D12_CULL_MODE_NONE  },
  { CullMode::Back,         D3D12_CULL_MODE_BACK  }, // use this as a key in reverse map
  { CullMode::Front,        D3D12_CULL_MODE_FRONT },
  { CullMode::FrontAndBack, D3D12_CULL_MODE_BACK  }  // Not supported, fallback to BACK
};

static const std::unordered_map<BlendFactor, D3D12_BLEND> blendFactorMapping = {
  { BlendFactor::Zero,                  D3D12_BLEND_ZERO             },
  { BlendFactor::One,                   D3D12_BLEND_ONE              },
  { BlendFactor::SrcColor,              D3D12_BLEND_SRC_COLOR        },
  { BlendFactor::OneMinusSrcColor,      D3D12_BLEND_INV_SRC_COLOR    },
  { BlendFactor::DstColor,              D3D12_BLEND_DEST_COLOR       },
  { BlendFactor::OneMinusDstColor,      D3D12_BLEND_INV_DEST_COLOR   },
  { BlendFactor::SrcAlpha,              D3D12_BLEND_SRC_ALPHA        },
  { BlendFactor::OneMinusSrcAlpha,      D3D12_BLEND_INV_SRC_ALPHA    },
  { BlendFactor::DstAlpha,              D3D12_BLEND_DEST_ALPHA       },
  { BlendFactor::OneMinusDstAlpha,      D3D12_BLEND_INV_DEST_ALPHA   },
  { BlendFactor::ConstantColor,         D3D12_BLEND_BLEND_FACTOR     },
  { BlendFactor::OneMinusConstantColor, D3D12_BLEND_INV_BLEND_FACTOR },
  { BlendFactor::SrcAlphaSaturate,      D3D12_BLEND_SRC_ALPHA_SAT    }
};

static const std::unordered_map<BlendOp, D3D12_BLEND_OP> blendOpMapping = {
  { BlendOp::Add,             D3D12_BLEND_OP_ADD          },
  { BlendOp::Subtract,        D3D12_BLEND_OP_SUBTRACT     },
  { BlendOp::ReverseSubtract, D3D12_BLEND_OP_REV_SUBTRACT },
  { BlendOp::MinValue,        D3D12_BLEND_OP_MIN          },
  { BlendOp::MaxValue,        D3D12_BLEND_OP_MAX          }
};

static const std::unordered_map<LogicOp, D3D12_LOGIC_OP> logicOpMapping = {
  { LogicOp::Clear,        D3D12_LOGIC_OP_CLEAR         },
  { LogicOp::And,          D3D12_LOGIC_OP_AND           },
  { LogicOp::AndReverse,   D3D12_LOGIC_OP_AND_REVERSE   },
  { LogicOp::Copy,         D3D12_LOGIC_OP_COPY          },
  { LogicOp::AndInverted,  D3D12_LOGIC_OP_AND_INVERTED  },
  { LogicOp::NoOp,         D3D12_LOGIC_OP_NOOP          },
  { LogicOp::Xor,          D3D12_LOGIC_OP_XOR           },
  { LogicOp::Or,           D3D12_LOGIC_OP_OR            },
  { LogicOp::Nor,          D3D12_LOGIC_OP_NOR           },
  { LogicOp::Equivalent,   D3D12_LOGIC_OP_EQUIV         },
  { LogicOp::Invert,       D3D12_LOGIC_OP_INVERT        },
  { LogicOp::OrReverse,    D3D12_LOGIC_OP_OR_REVERSE    },
  { LogicOp::CopyInverted, D3D12_LOGIC_OP_COPY_INVERTED },
  { LogicOp::OrInverted,   D3D12_LOGIC_OP_OR_INVERTED   },
  { LogicOp::Nand,         D3D12_LOGIC_OP_NAND          },
  { LogicOp::Set,          D3D12_LOGIC_OP_SET           }
};

static const std::unordered_map<ResourceLayout, D3D12_RESOURCE_STATES> resourceLayoutMapping = {
  { ResourceLayout::Undefined,                      D3D12_RESOURCE_STATE_COMMON                                                                                                   },
  { ResourceLayout::General,                        D3D12_RESOURCE_STATE_COMMON                                                                                                   },
  { ResourceLayout::Uav,                            D3D12_RESOURCE_STATE_UNORDERED_ACCESS                                                                                         },
  { ResourceLayout::ColorAttachment,                D3D12_RESOURCE_STATE_RENDER_TARGET                                                                                            },
  { ResourceLayout::DepthStencilAttachment,         D3D12_RESOURCE_STATE_DEPTH_WRITE                                                                                              },
  { ResourceLayout::DepthStencilReadOnly,           D3D12_RESOURCE_STATE_DEPTH_READ                                                                                               },
  { ResourceLayout::ShaderReadOnly,                 D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE                                   },
  { ResourceLayout::TransferSrc,                    D3D12_RESOURCE_STATE_COPY_SOURCE                                                                                              },
  { ResourceLayout::TransferDst,                    D3D12_RESOURCE_STATE_COPY_DEST                                                                                                },
  { ResourceLayout::Preinitialized,                 D3D12_RESOURCE_STATE_COMMON                                                                                                   },
  { ResourceLayout::DepthReadOnlyStencilAttachment, D3D12_RESOURCE_STATE_DEPTH_WRITE                                                                                              },
  { ResourceLayout::DepthAttachmentStencilReadOnly, D3D12_RESOURCE_STATE_DEPTH_WRITE                                                                                              },
  { ResourceLayout::DepthAttachment,                D3D12_RESOURCE_STATE_DEPTH_WRITE                                                                                              },
  { ResourceLayout::DepthReadOnly,                  D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
  { ResourceLayout::StencilAttachment,              D3D12_RESOURCE_STATE_DEPTH_WRITE                                                                                              },
  { ResourceLayout::StencilReadOnly,                D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
  { ResourceLayout::PresentSrc,                     D3D12_RESOURCE_STATE_PRESENT                                                                                                  },
  { ResourceLayout::SharedPresent,                  D3D12_RESOURCE_STATE_PRESENT                                                                                                  },
  { ResourceLayout::ShadingRateNv,                  D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE                                                                                      },
  { ResourceLayout::FragmentDensityMapExt,          D3D12_RESOURCE_STATE_COMMON                                                                                                   },
  { ResourceLayout::ReadOnly,                       D3D12_RESOURCE_STATE_GENERIC_READ                                                                                             },
  { ResourceLayout::Attachment,                     D3D12_RESOURCE_STATE_RENDER_TARGET                                                                                            },
  { ResourceLayout::AccelerationStructure,          D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE                                                                        }
};

// clang-format on

int g_getTextureComponentCountDx12(TextureFormat type) {
  // clang-format off
  static const std::unordered_map<TextureFormat, int> textureComponentCountMapping = {
    { TextureFormat::Rgb8,       4 }, // RGB8 maps to RGBA8 in DX12
    { TextureFormat::Rgb32f,     4 }, // RGB32F maps to RGBA32F in DX12
    { TextureFormat::Rgb16f,     4 }, // RGB16F maps to RGBA16F in DX12
    { TextureFormat::R11g11b10f, 3 },
    
    { TextureFormat::Rgba8,      4 },
    { TextureFormat::Rgba16f,    4 },
    { TextureFormat::Rgba32f,    4 },
    { TextureFormat::Rgba8si,    4 },
    { TextureFormat::Rgba8ui,    4 },
    
    { TextureFormat::Bgra8,      4 },
    
    { TextureFormat::R8,         1 },
    { TextureFormat::R16f,       1 },
    { TextureFormat::R32f,       1 },
    { TextureFormat::R8ui,       1 },
    { TextureFormat::R32ui,      1 },
    
    { TextureFormat::Rg8,        2 },
    { TextureFormat::Rg16f,      2 },
    { TextureFormat::Rg32f,      2 },
    
    { TextureFormat::D16,        1 },
    { TextureFormat::D16S8,      2 },
    { TextureFormat::D24,        1 },
    { TextureFormat::D24S8,      2 },
    { TextureFormat::D32,        1 },
    { TextureFormat::D32S8,      2 }
  };
  // clang-format on

  return getEnumMapping(textureComponentCountMapping, type, 0);
}

int g_getTexturePixelSizeDx12(TextureFormat type) {
  // clang-format off
  static const std::unordered_map<TextureFormat, int> texturePixelSizeMapping = {
    { TextureFormat::Rgb8,        4 }, // RGB8 maps to RGBA8 in DX12
    { TextureFormat::Rgb32f,     16 }, // RGB32F maps to RGBA32F in DX12
    { TextureFormat::Rgb16f,      8 }, // RGB16F maps to RGBA16F in DX12
    { TextureFormat::R11g11b10f,  4 },

    { TextureFormat::Rgba8,       4 },
    { TextureFormat::Rgba16f,     8 },
    { TextureFormat::Rgba32f,    16 },
    { TextureFormat::Rgba8si,     4 },
    { TextureFormat::Rgba8ui,     4 },

    { TextureFormat::Bgra8,       4 },

    { TextureFormat::R8,          1 },
    { TextureFormat::R16f,        2 },
    { TextureFormat::R32f,        4 },
    { TextureFormat::R8ui,        1 },
    { TextureFormat::R32ui,       4 },

    { TextureFormat::Rg8,         2 },
    { TextureFormat::Rg16f,       2 },
    { TextureFormat::Rg32f,       4 },

    { TextureFormat::D16,         2 },
    { TextureFormat::D16S8,       3 },
    { TextureFormat::D24,         3 },
    { TextureFormat::D24S8,       4 },
    { TextureFormat::D32,         4 },
    { TextureFormat::D32S8,       5 }
  };
  // clang-format on

  return getEnumMapping(texturePixelSizeMapping, type, 0);
}

TextureType g_getTextureDimensionDx12(D3D12_RESOURCE_DIMENSION type, bool isArray) {
  switch (type) {
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
      return isArray ? TextureType::Texture1DArray : TextureType::Texture1D;
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
      return isArray ? TextureType::Texture2DArray : TextureType::Texture2D;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
      return isArray ? TextureType::Texture3DArray : TextureType::Texture3D;
    case D3D12_RESOURCE_DIMENSION_UNKNOWN:
    case D3D12_RESOURCE_DIMENSION_BUFFER:
    default:
      return TextureType::Texture2D;  // Default to 2D texture
  }
}

D3D12_RESOURCE_DIMENSION g_getTextureDimensionDx12(TextureType type) {
  switch (type) {
    case TextureType::Texture1D:
    case TextureType::Texture1DArray:
      return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    case TextureType::Texture2D:
    case TextureType::Texture2DArray:
    case TextureType::TextureCube:
      return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    case TextureType::Texture3D:
    case TextureType::Texture3DArray:
      return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    default:
      return D3D12_RESOURCE_DIMENSION_UNKNOWN;
  }
}

uint8_t g_getColorMaskDx12(ColorMask type) {
  uint8_t result = 0;

  if (type == ColorMask::None) {
    return result;
  }

  if (type == ColorMask::All) {
    result = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE
           | D3D12_COLOR_WRITE_ENABLE_ALPHA;
  } else {
    if ((type & ColorMask::R) != ColorMask::None) {
      result |= D3D12_COLOR_WRITE_ENABLE_RED;
    }
    if ((type & ColorMask::G) != ColorMask::None) {
      result |= D3D12_COLOR_WRITE_ENABLE_GREEN;
    }
    if ((type & ColorMask::B) != ColorMask::None) {
      result |= D3D12_COLOR_WRITE_ENABLE_BLUE;
    }
    if ((type & ColorMask::A) != ColorMask::None) {
      result |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
    }
  }
  return result;
}

void g_getDepthFormatForSRV(DXGI_FORMAT& texFormat, DXGI_FORMAT& srvFormat, DXGI_FORMAT originalTexFormat) {
  switch (originalTexFormat) {
    case DXGI_FORMAT_D16_UNORM:
      texFormat = DXGI_FORMAT_R16_TYPELESS;
      srvFormat = DXGI_FORMAT_R16_UNORM;
      break;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
      texFormat = DXGI_FORMAT_R24G8_TYPELESS;
      srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
      break;
    case DXGI_FORMAT_D32_FLOAT:
      texFormat = DXGI_FORMAT_R32_TYPELESS;
      srvFormat = DXGI_FORMAT_R32_FLOAT;
      break;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      texFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
      srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
      break;
    default:
      assert(0);
      break;
  }
}

D3D12_FILTER g_getTextureFilterDx12(TextureFilter minification, TextureFilter magnification, bool isComparison) {
  if (isComparison) {
    D3D12_FILTER filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
    switch (minification) {
      case TextureFilter::Nearest:
      case TextureFilter::NearestMipmapNearest:
        if (magnification == TextureFilter::Nearest) {
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
        } else if (magnification == TextureFilter::Linear) {
          filter = D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
        } else if (magnification == TextureFilter::NearestMipmapNearest) {
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
        } else if (magnification == TextureFilter::LinearMipmapNearest) {
          filter = D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
        } else if (magnification == TextureFilter::NearestMipmapLinear) {
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapLinear) {
          filter = D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
        }
        break;
      case TextureFilter::Linear:
      case TextureFilter::LinearMipmapNearest:
        if (magnification == TextureFilter::Nearest) {
          filter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
        } else if (magnification == TextureFilter::Linear) {
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        } else if (magnification == TextureFilter::NearestMipmapNearest) {
          filter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
        } else if (magnification == TextureFilter::LinearMipmapNearest) {
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        } else if (magnification == TextureFilter::NearestMipmapLinear) {
          filter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapLinear) {
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        }
        break;
      case TextureFilter::NearestMipmapLinear:
        if (magnification == TextureFilter::Nearest) {
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::Linear) {
          filter = D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
        } else if (magnification == TextureFilter::NearestMipmapNearest) {
          // If mipmaps overlap, upload them as linear and process them.
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapNearest) {
          // If mipmaps overlap, upload them as linear and process them.
          filter = D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
        } else if (magnification == TextureFilter::NearestMipmapLinear) {
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapLinear) {
          filter = D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
        }
        break;
      case TextureFilter::LinearMipmapLinear:
        if (magnification == TextureFilter::Nearest) {
          filter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::Linear) {
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        } else if (magnification == TextureFilter::NearestMipmapNearest) {
          // If mipmaps overlap, upload them as linear and process them.
          filter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapNearest) {
          // If mipmaps overlap, upload them as linear and process them.
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        } else if (magnification == TextureFilter::NearestMipmapLinear) {
          filter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapLinear) {
          filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        }
        break;
      default:
        break;
    }
    return filter;
  } else {
    D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    switch (minification) {
      case TextureFilter::Nearest:
      case TextureFilter::NearestMipmapNearest:
        if (magnification == TextureFilter::Nearest) {
          filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        } else if (magnification == TextureFilter::Linear) {
          filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        } else if (magnification == TextureFilter::NearestMipmapNearest) {
          filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        } else if (magnification == TextureFilter::LinearMipmapNearest) {
          filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        } else if (magnification == TextureFilter::NearestMipmapLinear) {
          filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapLinear) {
          filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        }
        break;
      case TextureFilter::Linear:
      case TextureFilter::LinearMipmapNearest:
        if (magnification == TextureFilter::Nearest) {
          filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        } else if (magnification == TextureFilter::Linear) {
          filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        } else if (magnification == TextureFilter::NearestMipmapNearest) {
          filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        } else if (magnification == TextureFilter::LinearMipmapNearest) {
          filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        } else if (magnification == TextureFilter::NearestMipmapLinear) {
          filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapLinear) {
          filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        }
        break;
      case TextureFilter::NearestMipmapLinear:
        if (magnification == TextureFilter::Nearest) {
          filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::Linear) {
          filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        } else if (magnification == TextureFilter::NearestMipmapNearest) {
          // If mipmaps overlap, upload them as linear and process them
          filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapNearest) {
          // If mipmaps overlap, upload them as linear and process them
          filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        } else if (magnification == TextureFilter::NearestMipmapLinear) {
          filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapLinear) {
          filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        }
        break;
      case TextureFilter::LinearMipmapLinear:
        if (magnification == TextureFilter::Nearest) {
          filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::Linear) {
          filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        } else if (magnification == TextureFilter::NearestMipmapNearest) {
          // If mipmaps overlap, upload them as linear and process them
          filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapNearest) {
          // If mipmaps overlap, upload them as linear and process them
          filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        } else if (magnification == TextureFilter::NearestMipmapLinear) {
          filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        } else if (magnification == TextureFilter::LinearMipmapLinear) {
          filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        }
        break;
      default:
        break;
    }
    return filter;
  }
}

D3D12_SHADER_VISIBILITY g_getShaderVisibilityDx12(ShaderStageFlag stageFlags) {
  // Note: DirectX 12 doesn't support combining multiple visibilities except through ALL
  if (stageFlags == ShaderStageFlag::Vertex) {
    return D3D12_SHADER_VISIBILITY_VERTEX;
  } else if (stageFlags == ShaderStageFlag::Fragment) {
    return D3D12_SHADER_VISIBILITY_PIXEL;
  } else if (stageFlags == ShaderStageFlag::TessellationControl) {
    return D3D12_SHADER_VISIBILITY_HULL;
  } else if (stageFlags == ShaderStageFlag::TessellationEvaluation) {
    return D3D12_SHADER_VISIBILITY_DOMAIN;
  } else if (stageFlags == ShaderStageFlag::Geometry) {
    return D3D12_SHADER_VISIBILITY_GEOMETRY;
  }
  return D3D12_SHADER_VISIBILITY_ALL;
}

// clang-format off

static const std::unordered_map<DXGI_FORMAT, TextureFormat> textureFormatMappingToGeneric = reverseMap(textureFormatMappingToDXGI, 
  std::unordered_map<DXGI_FORMAT, TextureFormat>{
   { DXGI_FORMAT_R8G8B8A8_UNORM,     TextureFormat::Rgba8   },
   { DXGI_FORMAT_R16G16B16A16_FLOAT, TextureFormat::Rgba16f },
   { DXGI_FORMAT_D32_FLOAT,          TextureFormat::D32     },
   { DXGI_FORMAT_D24_UNORM_S8_UINT,  TextureFormat::D24S8   }
});

static const std::unordered_map<D3D12_RESOURCE_STATES, ResourceLayout> resourceLayoutReverseMapping = reverseMap(resourceLayoutMapping,
  std::unordered_map<D3D12_RESOURCE_STATES, ResourceLayout>{
    { D3D12_RESOURCE_STATE_COMMON,           ResourceLayout::Undefined              }, // or General
    { D3D12_RESOURCE_STATE_RENDER_TARGET,    ResourceLayout::ColorAttachment        }, // or just Attachment
    { D3D12_RESOURCE_STATE_DEPTH_WRITE,      ResourceLayout::DepthStencilAttachment }, // or DepthAttachment
    { D3D12_RESOURCE_STATE_DEPTH_READ,       ResourceLayout::DepthStencilReadOnly   },
    { D3D12_RESOURCE_STATE_PRESENT,          ResourceLayout::PresentSrc             }
});

// clang-format on

DXGI_FORMAT g_getTextureFormatDx12(TextureFormat textureFormat) {
  return getEnumMapping(textureFormatMappingToDXGI, textureFormat, DXGI_FORMAT_UNKNOWN);
}

TextureFormat g_getTextureFormatDx12(DXGI_FORMAT formatType) {
  return getEnumMapping(textureFormatMappingToGeneric, formatType, TextureFormat::Count);
}

D3D12_DESCRIPTOR_RANGE_TYPE g_getShaderBindingTypeDx12(ShaderBindingType bindingType) {
  return getEnumMapping(bindingTypeMapping, bindingType, D3D12_DESCRIPTOR_RANGE_TYPE_CBV);
}

D3D12_TEXTURE_ADDRESS_MODE g_getTextureAddressModeDx12(TextureAddressMode addressMode) {
  return getEnumMapping(addressModeMapping, addressMode, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}

D3D12_COMPARISON_FUNC g_getCompareOpDx12(CompareOp compareOp) {
  return getEnumMapping(compareOpMapping, compareOp, D3D12_COMPARISON_FUNC_NEVER);
}

D3D12_STENCIL_OP g_getStencilOpDx12(StencilOp stencilOp) {
  return getEnumMapping(stencilOpMapping, stencilOp, D3D12_STENCIL_OP_KEEP);
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE g_getPrimitiveTopologyTypeOnlyDx12(PrimitiveType type) {
  return getEnumMapping(primitiveTopologyTypeMapping, type, D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED);
}

D3D_PRIMITIVE_TOPOLOGY g_getPrimitiveTopologyDx12(PrimitiveType type) {
  return getEnumMapping(primitiveTopologyMapping, type, D3D_PRIMITIVE_TOPOLOGY_UNDEFINED);
}

D3D12_INPUT_CLASSIFICATION g_getVertexInputRateDx12(VertexInputRate inputRate) {
  return getEnumMapping(vertexInputRateMapping, inputRate, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA);
}

D3D12_FILL_MODE g_getPolygonModeDx12(PolygonMode polygonMode) {
  return getEnumMapping(polygonModeMapping, polygonMode, D3D12_FILL_MODE_SOLID);
}

D3D12_CULL_MODE g_getCullModeDx12(CullMode cullMode) {
  return getEnumMapping(cullModeMapping, cullMode, D3D12_CULL_MODE_NONE);
}

D3D12_BLEND g_getBlendFactorDx12(BlendFactor type) {
  return getEnumMapping(blendFactorMapping, type, D3D12_BLEND_ZERO);
}

D3D12_BLEND_OP g_getBlendOpDx12(BlendOp blendOp) {
  return getEnumMapping(blendOpMapping, blendOp, D3D12_BLEND_OP_ADD);
}

D3D12_LOGIC_OP g_getLogicOpDx12(LogicOp logicOp) {
  return getEnumMapping(logicOpMapping, logicOp, D3D12_LOGIC_OP_NOOP);
}

D3D12_RESOURCE_STATES g_getResourceLayoutDx12(ResourceLayout resourceLayout) {
  return getEnumMapping(resourceLayoutMapping, resourceLayout, D3D12_RESOURCE_STATE_COMMON);
}

ResourceLayout g_getResourceLayoutDx12(D3D12_RESOURCE_STATES resourceState) {
  return getEnumMapping(resourceLayoutReverseMapping, resourceState, ResourceLayout::Count);
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_RHI_DX12