#ifndef ARISE_RHI_TYPES_H
#define ARISE_RHI_TYPES_H

#include "gfx/rhi/common/rhi_enums.h"

#include <cstdint>
#include <string>
#include <vector>

namespace arise {
class Window;

namespace gfx {
namespace rhi {

class Buffer;
class Texture;
class Sampler;
class Shader;
class DescriptorSetLayout;
class DescriptorSet;
class RenderPass;
class Framebuffer;
class CommandBuffer;
class Fence;
class Semaphore;
class SwapChain;

// TODO:
// - for most x/y/z parameters, width/height/depth, consider using dimension/point class from math_library
// - consider adding explanatory comments for each parameter

struct DeviceDesc {
  Window* window = nullptr;  // Window for presenting
};

//------------------------------------------------------
// Resource descriptors
//------------------------------------------------------
struct BufferDesc {
  uint64_t         size        = 0;
  BufferCreateFlag createFlags = BufferCreateFlag::None;
  BufferType       type        = BufferType::Static;
  uint32_t         stride      = 0;  // Used only for vertex/index/instance buffers, ignored for other types
  std::string      debugName   = "";
};

struct TextureDesc {
  TextureType       type          = TextureType::Texture2D;
  TextureFormat     format        = TextureFormat::Rgba8;
  TextureCreateFlag createFlags   = TextureCreateFlag::None;
  uint32_t          width         = 1;
  uint32_t          height        = 1;
  uint32_t          depth         = 1;
  uint32_t          arraySize     = 1;
  uint32_t          mipLevels     = 1;
  MSAASamples       sampleCount   = MSAASamples::Count1;
  ResourceLayout    initialLayout = ResourceLayout::Undefined;
  std::string       debugName     = "";
};

struct SamplerDesc {
  TextureFilter      minFilter        = TextureFilter::Linear;
  TextureFilter      magFilter        = TextureFilter::Linear;
  TextureAddressMode addressModeU     = TextureAddressMode::Repeat;
  TextureAddressMode addressModeV     = TextureAddressMode::Repeat;
  TextureAddressMode addressModeW     = TextureAddressMode::Repeat;
  float              mipLodBias       = 0.0f;
  bool               anisotropyEnable = false;
  float              maxAnisotropy    = 1.0f;
  bool               compareEnable    = false;
  CompareOp          compareOp        = CompareOp::Never;
  float              minLod           = 0.0f;
  float              maxLod           = 1000.0f;
  float              borderColor[4]   = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct ShaderDesc {
  ShaderStageFlag      stage;
  std::vector<uint8_t> code;
  std::string          entryPoint = "main";
};

//------------------------------------------------------
// Vertex input descriptions
//------------------------------------------------------

// TODO: add comment (vk / dx12) to the struct name (what it means in the context of the API)
struct VertexInputBindingDesc {
  uint32_t        binding   = 0;  // Vk binding or DX12 input slot (from which buffer to read)
  uint32_t        stride    = 0;  // size in bytes of each vertex in this buffer
  VertexInputRate inputRate = VertexInputRate::Vertex;
};

struct VertexInputAttributeDesc {
  uint32_t     location = 0;  // shader - location in vk, semantic index in dx12
  uint32_t     binding  = 0;  // binding in vk, input slot in dx12
  VertexFormat format   = VertexFormat::Rgba8;
  uint32_t     offset   = 0;

  // DirectX 12 specific semantic name (ignored in Vulkan)
  std::string semanticName
      = "FORGOT_TO_SET_SEMANTIC_NAME";  // set this default values so it will be easier to spot the error
};

//------------------------------------------------------
// Pipeline state descriptions
//------------------------------------------------------
struct Viewport {
  float x        = 0.0f;
  float y        = 0.0f;
  float width    = 0.0f;
  float height   = 0.0f;
  float minDepth = 0.0f;
  float maxDepth = 1.0f;
};

struct ScissorRect {
  int32_t  x      = 0;
  int32_t  y      = 0;
  uint32_t width  = 0;
  uint32_t height = 0;
};

struct InputAssemblyStateDesc {
  PrimitiveType topology               = PrimitiveType::Triangles;
  bool          primitiveRestartEnable = false;
};

struct RasterizationStateDesc {
  bool        depthClampEnable        = false;
  bool        rasterizerDiscardEnable = false;
  PolygonMode polygonMode             = PolygonMode::Fill;
  CullMode    cullMode                = CullMode::Back;
  FrontFace   frontFace               = FrontFace::Ccw;
  bool        depthBiasEnable         = false;
  float       depthBiasConstantFactor = 0.0f;
  float       depthBiasClamp          = 0.0f;
  float       depthBiasSlopeFactor    = 0.0f;
  float       lineWidth               = 1.0f;
};

struct StencilOpState {
  StencilOp failOp      = StencilOp::Keep;
  StencilOp passOp      = StencilOp::Keep;
  StencilOp depthFailOp = StencilOp::Keep;
  CompareOp compareOp   = CompareOp::Always;
  uint32_t  compareMask = 0xFF;
  uint32_t  writeMask   = 0xFF;
  uint32_t  reference   = 0;
};

struct DepthStencilStateDesc {
  bool      depthTestEnable       = true;
  bool      depthWriteEnable      = true;
  CompareOp depthCompareOp        = CompareOp::Less;
  bool      depthBoundsTestEnable = false;
  bool      stencilTestEnable     = false;

  StencilOpState front;
  StencilOpState back;
  float          minDepthBounds = 0.0f;
  float          maxDepthBounds = 1.0f;
};

struct ColorBlendAttachmentDesc {
  bool        blendEnable         = false;
  BlendFactor srcColorBlendFactor = BlendFactor::One;
  BlendFactor dstColorBlendFactor = BlendFactor::Zero;
  BlendOp     colorBlendOp        = BlendOp::Add;
  BlendFactor srcAlphaBlendFactor = BlendFactor::One;
  BlendFactor dstAlphaBlendFactor = BlendFactor::Zero;
  BlendOp     alphaBlendOp        = BlendOp::Add;
  ColorMask   colorWriteMask      = ColorMask::All;
};

struct ColorBlendStateDesc {
  bool                                  logicOpEnable = false;
  LogicOp                               logicOp       = LogicOp::Copy;
  std::vector<ColorBlendAttachmentDesc> attachments;
  float                                 blendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct MultisampleStateDesc {
  MSAASamples rasterizationSamples  = MSAASamples::Count1;
  bool        sampleShadingEnable   = false;
  float       minSampleShading      = 0.0f;
  uint32_t    sampleMask            = 0xFF'FF'FF'FF;
  bool        alphaToCoverageEnable = false;
  bool        alphaToOneEnable      = false;
};

//------------------------------------------------------
// Descriptor set layout
//------------------------------------------------------
struct DescriptorSetLayoutBindingDesc {
  uint32_t          binding         = 0;
  ShaderBindingType type            = ShaderBindingType::Uniformbuffer;
  uint32_t          descriptorCount = 1;
  ShaderStageFlag   stageFlags      = ShaderStageFlag::All;
};

struct DescriptorSetLayoutDesc {
  std::vector<DescriptorSetLayoutBindingDesc> bindings;
};

//------------------------------------------------------
// Pipeline descriptions
//------------------------------------------------------
struct GraphicsPipelineDesc {
  std::vector<Shader*> shaders;

  // Fixed function state
  std::vector<VertexInputBindingDesc>   vertexBindings;
  std::vector<VertexInputAttributeDesc> vertexAttributes;
  InputAssemblyStateDesc                inputAssembly;
  RasterizationStateDesc                rasterization;
  DepthStencilStateDesc                 depthStencil;
  ColorBlendStateDesc                   colorBlend;
  MultisampleStateDesc                  multisample;

  std::vector<const DescriptorSetLayout*> setLayouts;
  RenderPass*                             renderPass = nullptr;
  uint32_t                                subpass    = 0;
};

//------------------------------------------------------
// Render pass and framebuffer
//------------------------------------------------------
struct RenderPassAttachmentDesc {
  TextureFormat         format             = TextureFormat::Rgba8;
  MSAASamples           samples            = MSAASamples::Count1;
  AttachmentLoadStoreOp loadStoreOp        = AttachmentLoadStoreOp::ClearStore;
  AttachmentLoadStoreOp stencilLoadStoreOp = AttachmentLoadStoreOp::DontcareDontcare;
  ResourceLayout        initialLayout      = ResourceLayout::Undefined;
  ResourceLayout        finalLayout        = ResourceLayout::ShaderReadOnly;
};

struct RenderPassDesc {
  std::vector<RenderPassAttachmentDesc> colorAttachments;
  RenderPassAttachmentDesc              depthStencilAttachment;
  bool                                  hasDepthStencil = false;
};

struct FramebufferDesc {
  uint32_t              width  = 0;
  uint32_t              height = 0;
  std::vector<Texture*> colorAttachments;
  Texture*              depthStencilAttachment = nullptr;
  bool                  hasDepthStencil        = false;
  RenderPass*           renderPass             = nullptr;
};

//------------------------------------------------------
// Swapchain, command buffers, synchronization
//------------------------------------------------------
struct SwapchainDesc {
  uint32_t      width       = 0;
  uint32_t      height      = 0;
  TextureFormat format      = TextureFormat::Bgra8;
  uint32_t      bufferCount = 2;
};

struct CommandBufferDesc {
  bool primary = true;
};

struct FenceDesc {
  bool signaled = false;
};

//------------------------------------------------------
// Other structures
//------------------------------------------------------
struct ResourceBarrierDesc {
  Texture*       texture   = nullptr;
  ResourceLayout oldLayout = ResourceLayout::Undefined;
  ResourceLayout newLayout = ResourceLayout::General;
};

union ClearValue {
  float color[4];

  struct {
    float    depth;
    uint32_t stencil;
  } depthStencil;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_RHI_TYPES_H