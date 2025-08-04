#include "gfx/rhi/backends/vulkan/rhi_enums_vk.h"

#include <cassert>

namespace arise {
namespace gfx {
namespace rhi {

// clang-format off

static const std::unordered_map<TextureFilter, VkFilter> filterMapping = {
  { TextureFilter::Nearest,              VK_FILTER_NEAREST }, // use this as a key in reverse map
  { TextureFilter::Linear,               VK_FILTER_LINEAR  }, // use this as a key in reverse map
  { TextureFilter::NearestMipmapNearest, VK_FILTER_NEAREST },  
  { TextureFilter::LinearMipmapNearest,  VK_FILTER_LINEAR  },  
  { TextureFilter::NearestMipmapLinear,  VK_FILTER_NEAREST },  
  { TextureFilter::LinearMipmapLinear,   VK_FILTER_LINEAR  }   
};

static const std::unordered_map<TextureAddressMode, VkSamplerAddressMode> addressModeMapping = {
  { TextureAddressMode::Repeat,            VK_SAMPLER_ADDRESS_MODE_REPEAT               },
  { TextureAddressMode::MirroredRepeat,    VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT      },
  { TextureAddressMode::ClampToEdge,       VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE        },
  { TextureAddressMode::ClampToBorder,     VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER      },
  { TextureAddressMode::MirrorClampToEdge, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE }
};

static const std::unordered_map<TextureFilter, VkSamplerMipmapMode> mipmapModeMapping = {
  { TextureFilter::Nearest,              VK_SAMPLER_MIPMAP_MODE_NEAREST }, // use this as a key in reverse map
  { TextureFilter::Linear,               VK_SAMPLER_MIPMAP_MODE_NEAREST },  
  { TextureFilter::NearestMipmapNearest, VK_SAMPLER_MIPMAP_MODE_NEAREST },  
  { TextureFilter::LinearMipmapNearest,  VK_SAMPLER_MIPMAP_MODE_NEAREST },  
  { TextureFilter::NearestMipmapLinear,  VK_SAMPLER_MIPMAP_MODE_LINEAR  },  // use this as a key in reverse map
  { TextureFilter::LinearMipmapLinear,   VK_SAMPLER_MIPMAP_MODE_LINEAR  }   
};

static const std::unordered_map<TextureFormat, VkFormat> formatMapping = {
  { TextureFormat::Rgb8,       VK_FORMAT_R8G8B8_UNORM            },
  { TextureFormat::Rgb32f,     VK_FORMAT_R32G32B32_SFLOAT        },
  { TextureFormat::Rgb16f,     VK_FORMAT_R16G16B16_SFLOAT        },
  { TextureFormat::R11g11b10f, VK_FORMAT_B10G11R11_UFLOAT_PACK32 },
  { TextureFormat::Rgba8,      VK_FORMAT_R8G8B8A8_UNORM          },
  { TextureFormat::Rgba16f,    VK_FORMAT_R16G16B16A16_SFLOAT     },
  { TextureFormat::Rgba32f,    VK_FORMAT_R32G32B32A32_SFLOAT     },
  { TextureFormat::Rgba8si,    VK_FORMAT_R8G8B8A8_SINT           },
  { TextureFormat::Rgba8ui,    VK_FORMAT_R8G8B8A8_UINT           },
  { TextureFormat::Bgra8,      VK_FORMAT_B8G8R8A8_UNORM          },
  { TextureFormat::R8,         VK_FORMAT_R8_UNORM                },
  { TextureFormat::R16f,       VK_FORMAT_R16_SFLOAT              },
  { TextureFormat::R32f,       VK_FORMAT_R32_SFLOAT              },
  { TextureFormat::R8ui,       VK_FORMAT_R8_UINT                 },
  { TextureFormat::R32ui,      VK_FORMAT_R32_UINT                },
  { TextureFormat::Rg8,        VK_FORMAT_R8G8_UNORM              },
  { TextureFormat::Rg16f,      VK_FORMAT_R16G16_SFLOAT           },
  { TextureFormat::Rg32f,      VK_FORMAT_R32G32_SFLOAT           },
  { TextureFormat::D16,        VK_FORMAT_D16_UNORM               },
  { TextureFormat::D16S8,      VK_FORMAT_D16_UNORM_S8_UINT       },
  { TextureFormat::D24,        VK_FORMAT_X8_D24_UNORM_PACK32     },
  { TextureFormat::D24S8,      VK_FORMAT_D24_UNORM_S8_UINT       },
  { TextureFormat::D32,        VK_FORMAT_D32_SFLOAT              },
  { TextureFormat::D32S8,      VK_FORMAT_D32_SFLOAT_S8_UINT      },
  { TextureFormat::Bc1Unorm,   VK_FORMAT_BC1_RGBA_UNORM_BLOCK    },
  { TextureFormat::Bc2Unorm,   VK_FORMAT_BC2_UNORM_BLOCK         },
  { TextureFormat::Bc3Unorm,   VK_FORMAT_BC3_UNORM_BLOCK         },
  { TextureFormat::Bc4Unorm,   VK_FORMAT_BC4_UNORM_BLOCK         },
  { TextureFormat::Bc4Snorm,   VK_FORMAT_BC4_SNORM_BLOCK         },
  { TextureFormat::Bc5Unorm,   VK_FORMAT_BC5_UNORM_BLOCK         },
  { TextureFormat::Bc5Snorm,   VK_FORMAT_BC5_SNORM_BLOCK         },
  { TextureFormat::Bc6hUf16,   VK_FORMAT_BC6H_UFLOAT_BLOCK       },
  { TextureFormat::Bc6hSf16,   VK_FORMAT_BC6H_SFLOAT_BLOCK       },
  { TextureFormat::Bc7Unorm,   VK_FORMAT_BC7_UNORM_BLOCK         }
};

static const std::unordered_map<PrimitiveType, VkPrimitiveTopology> topologyMapping = {
  { PrimitiveType::Points,                 VK_PRIMITIVE_TOPOLOGY_POINT_LIST                    },
  { PrimitiveType::Lines,                  VK_PRIMITIVE_TOPOLOGY_LINE_LIST                     },
  { PrimitiveType::LinesAdjacency,         VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY      },
  { PrimitiveType::LineStripAdjacency,     VK_PRIMITIVE_TOPOLOGY_LINE_STRIP                    },
  { PrimitiveType::Triangles,              VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST                 },
  { PrimitiveType::TrianglesAdjacency,     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY  },
  { PrimitiveType::TriangleStripAdjacency, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY },
  { PrimitiveType::TriangleStrip,          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP                }
};

static const std::unordered_map<VertexInputRate, VkVertexInputRate> inputRateMapping = {
  { VertexInputRate::Vertex,   VK_VERTEX_INPUT_RATE_VERTEX   },
  { VertexInputRate::Instance, VK_VERTEX_INPUT_RATE_INSTANCE }
};

static const std::unordered_map<PolygonMode, VkPolygonMode> polygonModeMapping = {
  { PolygonMode::Point, VK_POLYGON_MODE_POINT },
  { PolygonMode::Line,  VK_POLYGON_MODE_LINE  },
  { PolygonMode::Fill,  VK_POLYGON_MODE_FILL  }
};

static const std::unordered_map<FrontFace, VkFrontFace> frontFaceMapping = {
  { FrontFace::Cw,  VK_FRONT_FACE_CLOCKWISE         },
  { FrontFace::Ccw, VK_FRONT_FACE_COUNTER_CLOCKWISE }
};

static const std::unordered_map<StencilOp, VkStencilOp> stencilOpMapping = {
  { StencilOp::Keep,     VK_STENCIL_OP_KEEP                },
  { StencilOp::Zero,     VK_STENCIL_OP_ZERO                },
  { StencilOp::Replace,  VK_STENCIL_OP_REPLACE             },
  { StencilOp::Incr,     VK_STENCIL_OP_INCREMENT_AND_CLAMP },
  { StencilOp::IncrWrap, VK_STENCIL_OP_INCREMENT_AND_WRAP  },
  { StencilOp::Decr,     VK_STENCIL_OP_DECREMENT_AND_CLAMP },
  { StencilOp::DecrWrap, VK_STENCIL_OP_DECREMENT_AND_WRAP  },
  { StencilOp::Invert,   VK_STENCIL_OP_INVERT              }
};

static const std::unordered_map<CompareOp, VkCompareOp> compareOpMapping = {
  { CompareOp::Never,    VK_COMPARE_OP_NEVER            },
  { CompareOp::Less,     VK_COMPARE_OP_LESS             },
  { CompareOp::Equal,    VK_COMPARE_OP_EQUAL            },
  { CompareOp::LessEqual,   VK_COMPARE_OP_LESS_OR_EQUAL    },
  { CompareOp::Greater,  VK_COMPARE_OP_GREATER          },
  { CompareOp::NotEqual, VK_COMPARE_OP_NOT_EQUAL        },
  { CompareOp::GreaterEqual,   VK_COMPARE_OP_GREATER_OR_EQUAL },
  { CompareOp::Always,   VK_COMPARE_OP_ALWAYS           }
};

static const std::unordered_map<BlendFactor, VkBlendFactor> blendFactorMapping = {
  { BlendFactor::Zero,                  VK_BLEND_FACTOR_ZERO                     },
  { BlendFactor::One,                   VK_BLEND_FACTOR_ONE                      },
  { BlendFactor::SrcColor,              VK_BLEND_FACTOR_SRC_COLOR                },
  { BlendFactor::OneMinusSrcColor,      VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR      },
  { BlendFactor::DstColor,              VK_BLEND_FACTOR_DST_COLOR                },
  { BlendFactor::OneMinusDstColor,      VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR      },
  { BlendFactor::SrcAlpha,              VK_BLEND_FACTOR_SRC_ALPHA                },
  { BlendFactor::OneMinusSrcAlpha,      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA      },
  { BlendFactor::DstAlpha,              VK_BLEND_FACTOR_DST_ALPHA                },
  { BlendFactor::OneMinusDstAlpha,      VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA      },
  { BlendFactor::ConstantColor,         VK_BLEND_FACTOR_CONSTANT_COLOR           },
  { BlendFactor::OneMinusConstantColor, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR },
  { BlendFactor::ConstantAlpha,         VK_BLEND_FACTOR_CONSTANT_ALPHA           },
  { BlendFactor::OneMinusConstantAlpha, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA },
  { BlendFactor::SrcAlphaSaturate,      VK_BLEND_FACTOR_SRC_ALPHA_SATURATE       }
};

static const std::unordered_map<BlendOp, VkBlendOp> blendOpMapping = {
  { BlendOp::Add,             VK_BLEND_OP_ADD              },
  { BlendOp::Subtract,        VK_BLEND_OP_SUBTRACT         },
  { BlendOp::ReverseSubtract, VK_BLEND_OP_REVERSE_SUBTRACT },
  { BlendOp::MinValue,        VK_BLEND_OP_MIN              },
  { BlendOp::MaxValue,        VK_BLEND_OP_MAX              }
};

static const std::unordered_map<LogicOp, VkLogicOp> logicOpMapping = {
  { LogicOp::Clear,        VK_LOGIC_OP_CLEAR         },
  { LogicOp::And,          VK_LOGIC_OP_AND           },
  { LogicOp::AndReverse,   VK_LOGIC_OP_AND_REVERSE   },
  { LogicOp::Copy,         VK_LOGIC_OP_COPY          },
  { LogicOp::AndInverted,  VK_LOGIC_OP_AND_INVERTED  },
  { LogicOp::NoOp,         VK_LOGIC_OP_NO_OP         },
  { LogicOp::Xor,          VK_LOGIC_OP_XOR           },
  { LogicOp::Or,           VK_LOGIC_OP_OR            },
  { LogicOp::Nor,          VK_LOGIC_OP_NOR           },
  { LogicOp::Equivalent,   VK_LOGIC_OP_EQUIVALENT    },
  { LogicOp::Invert,       VK_LOGIC_OP_INVERT        },
  { LogicOp::OrReverse,    VK_LOGIC_OP_OR_REVERSE    },
  { LogicOp::CopyInverted, VK_LOGIC_OP_COPY_INVERTED },
  { LogicOp::OrInverted,   VK_LOGIC_OP_OR_INVERTED   },
  { LogicOp::Nand,         VK_LOGIC_OP_NAND          },
  { LogicOp::Set,          VK_LOGIC_OP_SET           }
};

static const std::unordered_map<ResourceLayout, VkImageLayout> imageLayoutMapping = {
  { ResourceLayout::Undefined,                      VK_IMAGE_LAYOUT_UNDEFINED                                  },
  { ResourceLayout::General,                        VK_IMAGE_LAYOUT_GENERAL                                    },
  { ResourceLayout::Uav,                            VK_IMAGE_LAYOUT_GENERAL                                    },
  { ResourceLayout::ColorAttachment,                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL                   },
  { ResourceLayout::DepthStencilAttachment,         VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL           },
  { ResourceLayout::DepthStencilReadOnly,           VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL            },
  { ResourceLayout::ShaderReadOnly,                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                   },
  { ResourceLayout::TransferSrc,                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL                       },
  { ResourceLayout::TransferDst,                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL                       },
  { ResourceLayout::Preinitialized,                 VK_IMAGE_LAYOUT_PREINITIALIZED                             },
  { ResourceLayout::DepthReadOnlyStencilAttachment, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL },
  { ResourceLayout::DepthAttachmentStencilReadOnly, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL },
  { ResourceLayout::DepthAttachment,                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL                   },
  { ResourceLayout::DepthReadOnly,                  VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL                    },
  { ResourceLayout::StencilAttachment,              VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL                 },
  { ResourceLayout::StencilReadOnly,                VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL                  },
  { ResourceLayout::PresentSrc,                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR                            },
  { ResourceLayout::SharedPresent,                  VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR                         },
  { ResourceLayout::ShadingRateNv,                  VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV                    },
  { ResourceLayout::FragmentDensityMapExt,          VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT           },
  { ResourceLayout::ReadOnly,                       VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR                      },
  { ResourceLayout::Attachment,                     VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR                     },
  { ResourceLayout::AccelerationStructure,          VK_IMAGE_LAYOUT_GENERAL                                    } 
};

// clang-format on

int g_getTextureComponentCountVk(TextureFormat type) {
  // clang-format off
  static const std::unordered_map<TextureFormat, int> textureComponentCountMapping = {
    { TextureFormat::Rgb8,       3 },
    { TextureFormat::Rgb32f,     3 },
    { TextureFormat::Rgb16f,     3 },
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

int g_getTexturePixelSizeVk(TextureFormat type) {
  // clang-format off
  static const std::unordered_map<TextureFormat, int> texturePixelSizeMapping = {
    { TextureFormat::Rgb8,       3  },
    { TextureFormat::Rgb32f,     12 },
    { TextureFormat::Rgb16f,     6  },
    { TextureFormat::R11g11b10f, 4  },

    { TextureFormat::Rgba8,      4  },
    { TextureFormat::Rgba16f,    8  },
    { TextureFormat::Rgba32f,    16 },
    { TextureFormat::Rgba8si,    4  },
    { TextureFormat::Rgba8ui,    4  },

    { TextureFormat::Bgra8,      4  },
    
    { TextureFormat::R8,         1  },
    { TextureFormat::R16f,       2  },
    { TextureFormat::R32f,       4  },
    { TextureFormat::R8ui,       1  },
    { TextureFormat::R32ui,      4  },

    { TextureFormat::Rg8,        2  },
    { TextureFormat::Rg16f,      2  },
    { TextureFormat::Rg32f,      4  },

    { TextureFormat::D16,        2  },
    { TextureFormat::D16S8,      3  },
    { TextureFormat::D24,        3  },
    { TextureFormat::D24S8,      4  },
    { TextureFormat::D32,        4  },
    { TextureFormat::D32S8,      5  }
  };
  // clang-format on

  return getEnumMapping(texturePixelSizeMapping, type, 0);
}

void g_getAttachmentLoadStoreOpVk(VkAttachmentLoadOp&   loadOp,
                                  VkAttachmentStoreOp&  storeOp,
                                  AttachmentLoadStoreOp type) {
  switch (type) {
    case AttachmentLoadStoreOp::LoadStore:
      loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
      storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      break;
    case AttachmentLoadStoreOp::LoadDontcare:
      loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
      storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      break;
    case AttachmentLoadStoreOp::ClearStore:
      loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
      storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      break;
    case AttachmentLoadStoreOp::ClearDontcare:
      loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
      storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      break;
    case AttachmentLoadStoreOp::DontcareStore:
      loadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      break;
    case AttachmentLoadStoreOp::DontcareDontcare:
      loadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      break;
    default:
      assert(0);
      break;
  }
}

// TODO: rewrite to use std::unordered_map
VkShaderStageFlags g_getShaderStageFlagsVk(ShaderStageFlag stages) {
  VkShaderStageFlags result = 0;

  if ((stages & ShaderStageFlag::Vertex) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_VERTEX_BIT;
  }
  if ((stages & ShaderStageFlag::TessellationControl) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
  }
  if ((stages & ShaderStageFlag::TessellationEvaluation) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
  }
  if ((stages & ShaderStageFlag::Geometry) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_GEOMETRY_BIT;
  }
  if ((stages & ShaderStageFlag::Fragment) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  if ((stages & ShaderStageFlag::Compute) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_COMPUTE_BIT;
  }
  if ((stages & ShaderStageFlag::AllGraphics) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_ALL_GRAPHICS;
  }
  if ((stages & ShaderStageFlag::RaytracingRaygen) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  }
  if ((stages & ShaderStageFlag::RaytracingAnyhit) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
  }
  if ((stages & ShaderStageFlag::RaytracingClosesthit) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  }
  if ((stages & ShaderStageFlag::RaytracingMiss) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_MISS_BIT_KHR;
  }
  if ((stages & ShaderStageFlag::AllRaytracing) != ShaderStageFlag::None) {
    result |= VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
            | VK_SHADER_STAGE_MISS_BIT_KHR;
  }
  if ((stages & ShaderStageFlag::All) != ShaderStageFlag::None) {
    result = VK_SHADER_STAGE_ALL;
  }
  return result;
}

VkShaderStageFlagBits g_getShaderStageBitsVk(ShaderStageFlag type) {
  switch (type) {
    case ShaderStageFlag::Vertex:
      return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderStageFlag::TessellationControl:
      return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case ShaderStageFlag::TessellationEvaluation:
      return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    case ShaderStageFlag::Geometry:
      return VK_SHADER_STAGE_GEOMETRY_BIT;
    case ShaderStageFlag::Fragment:
      return VK_SHADER_STAGE_FRAGMENT_BIT;
    case ShaderStageFlag::Compute:
      return VK_SHADER_STAGE_COMPUTE_BIT;
    case ShaderStageFlag::AllGraphics:
      return VK_SHADER_STAGE_ALL_GRAPHICS;
    case ShaderStageFlag::RaytracingRaygen:
      return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    case ShaderStageFlag::RaytracingAnyhit:
      return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    case ShaderStageFlag::RaytracingClosesthit:
      return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case ShaderStageFlag::RaytracingMiss:
      return VK_SHADER_STAGE_MISS_BIT_KHR;
    case ShaderStageFlag::All:
    case ShaderStageFlag::AllRaytracing:
      return VK_SHADER_STAGE_ALL;
    default:
      assert(0);
      break;
  }
  return VK_SHADER_STAGE_ALL;
}

VkColorComponentFlags g_getColorMaskVk(ColorMask type) {
  VkColorComponentFlags result = 0;

  if (type == ColorMask::None) {
    return result;
  }

  if (type == ColorMask::All) {
    result = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  } else {
    if ((type & ColorMask::R) != ColorMask::None) {
      result |= VK_COLOR_COMPONENT_R_BIT;
    }
    if ((type & ColorMask::G) != ColorMask::None) {
      result |= VK_COLOR_COMPONENT_G_BIT;
    }
    if ((type & ColorMask::B) != ColorMask::None) {
      result |= VK_COLOR_COMPONENT_B_BIT;
    }
    if ((type & ColorMask::A) != ColorMask::None) {
      result |= VK_COLOR_COMPONENT_A_BIT;
    }
  }
  return result;
}

// TODO: rewrite to use std::unordered_map
VkDescriptorType g_getShaderBindingTypeVk(ShaderBindingType type) {
  switch (type) {
    case ShaderBindingType::Uniformbuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case ShaderBindingType::UniformbufferDynamic:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case ShaderBindingType::TextureSamplerSrv:
      return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case ShaderBindingType::TextureSrv:
      return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case ShaderBindingType::TextureUav:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case ShaderBindingType::TextureArraySrv:
      return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case ShaderBindingType::Sampler:
      return VK_DESCRIPTOR_TYPE_SAMPLER;
    case ShaderBindingType::BufferSrv:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ShaderBindingType::BufferUav:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ShaderBindingType::BufferUavDynamic:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case ShaderBindingType::BufferTexelSrv:
      return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    case ShaderBindingType::BufferTexelUav:
      return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    case ShaderBindingType::AccelerationStructureSrv:
      return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    case ShaderBindingType::SubpassInputAttachment:
      return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    default:
      assert(0);
      break;
  }
  assert(0);
  return VK_DESCRIPTOR_TYPE_SAMPLER;
}

// TODO: rewrite to use std::unordered_map
VkCullModeFlags g_getCullModeVk(CullMode cullMode) {
  switch (cullMode) {
    case CullMode::None:
      return VK_CULL_MODE_NONE;
    case CullMode::Back:
      return VK_CULL_MODE_BACK_BIT;
    case CullMode::Front:
      return VK_CULL_MODE_FRONT_BIT;
    case CullMode::FrontAndBack:
      return VK_CULL_MODE_FRONT_AND_BACK;
    default:
      assert(0);
      break;
  }
  return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
}

// clang-format off

static const std::unordered_map<VkImageLayout, ResourceLayout> imageLayoutReverseMapping = reverseMap(imageLayoutMapping,
  std::unordered_map<VkImageLayout, ResourceLayout>{
    { VK_IMAGE_LAYOUT_GENERAL, ResourceLayout::General }
});
static const std::unordered_map<VkFormat, TextureFormat> formatReverseMapping = reverseMap(formatMapping);

// clang-format on

VkFilter g_getTextureFilterTypeVk(TextureFilter textureFilter) {
  return getEnumMapping(filterMapping, textureFilter, VK_FILTER_MAX_ENUM);
}

VkSamplerAddressMode g_getTextureAddressModeVk(TextureAddressMode addressMode) {
  return getEnumMapping(addressModeMapping, addressMode, VK_SAMPLER_ADDRESS_MODE_MAX_ENUM);
}

VkSamplerMipmapMode g_getTextureMipmapModeVk(TextureFilter textureFilter) {
  return getEnumMapping(mipmapModeMapping, textureFilter, VK_SAMPLER_MIPMAP_MODE_MAX_ENUM);
}

VkFormat g_getTextureFormatVk(TextureFormat textureFormat) {
  return getEnumMapping(formatMapping, textureFormat, VK_FORMAT_UNDEFINED);
}

TextureFormat g_getTextureFormatVk(VkFormat format) {
  return getEnumMapping(formatReverseMapping, format, TextureFormat::Count);
}

VkPrimitiveTopology g_getPrimitiveTopologyVk(PrimitiveType primitiveType) {
  return getEnumMapping(topologyMapping, primitiveType, VK_PRIMITIVE_TOPOLOGY_MAX_ENUM);
}

VkVertexInputRate g_getVertexInputRateVk(VertexInputRate inputRate) {
  return getEnumMapping(inputRateMapping, inputRate, VK_VERTEX_INPUT_RATE_MAX_ENUM);
}

VkPolygonMode g_getPolygonModeVk(PolygonMode polygonMode) {
  return getEnumMapping(polygonModeMapping, polygonMode, VK_POLYGON_MODE_MAX_ENUM);
}

VkFrontFace g_getFrontFaceVk(FrontFace frontFace) {
  return getEnumMapping(frontFaceMapping, frontFace, VK_FRONT_FACE_MAX_ENUM);
}

VkStencilOp g_getStencilOpVk(StencilOp stencilOp) {
  return getEnumMapping(stencilOpMapping, stencilOp, VK_STENCIL_OP_MAX_ENUM);
}

VkCompareOp g_getCompareOpVk(CompareOp compareOp) {
  return getEnumMapping(compareOpMapping, compareOp, VK_COMPARE_OP_MAX_ENUM);
}

VkBlendFactor g_getBlendFactorVk(BlendFactor blendFactor) {
  return getEnumMapping(blendFactorMapping, blendFactor, VK_BLEND_FACTOR_MAX_ENUM);
}

VkBlendOp g_getBlendOpVk(BlendOp blendOp) {
  return getEnumMapping(blendOpMapping, blendOp, VK_BLEND_OP_MAX_ENUM);
}

VkLogicOp g_getLogicOpVk(LogicOp logicOp) {
  return getEnumMapping(logicOpMapping, logicOp, VK_LOGIC_OP_NO_OP);
}

VkImageLayout g_getImageLayoutVk(ResourceLayout resourceLayout) {
  return getEnumMapping(imageLayoutMapping, resourceLayout, VK_IMAGE_LAYOUT_MAX_ENUM);
}

ResourceLayout g_getImageLayoutVk(VkImageLayout resourceLayout) {
  return getEnumMapping(imageLayoutReverseMapping, resourceLayout, ResourceLayout::Count);
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise