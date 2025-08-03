#ifndef ARISE_SHADER_REFLECTION_TYPES_H
#define ARISE_SHADER_REFLECTION_TYPES_H

#include "gfx/rhi/common/rhi_types.h"
#include <vector>
#include <string>

namespace arise {
namespace gfx {
namespace rhi {

//------------------------------------------------------
// Shader reflection structures
//------------------------------------------------------

/**
 * Describes a single resource binding (texture, sampler, constant buffer) in a shader
 */
struct ShaderResourceBinding {
  uint32_t          binding         = 0;      // binding index in shader
  uint32_t          set             = 0;      // descriptor set index
  ShaderBindingType type            = ShaderBindingType::Uniformbuffer;
  uint32_t          descriptorCount = 1;      // array size
  ShaderStageFlag   stageFlags      = ShaderStageFlag::All;
  std::string       name            = "";     // resource name from shader
  bool              nonUniform      = false;  // for bindless arrays
};

/**
 * Contains vertex input variable information extracted from shader reflection
 */
struct ShaderVertexInput {
  uint32_t      location     = 0;                       // input location (Vulkan) / semantic index (DirectX)
  std::string   semanticName = "";                      // semantic name (DirectX) / inferred from location (Vulkan)
  TextureFormat format       = TextureFormat::Rgba32f;  // inferred format
  uint32_t      arraySize    = 1;                       // array size (1 if not array)
};

/**
 * Contains reflection data extracted from compiled shader
 */
struct ShaderMeta {
  std::vector<ShaderResourceBinding> bindings;
  std::vector<ShaderVertexInput>     vertexInputs;          // vertex shaders only
  uint32_t                           pushConstantSize = 0;  // size in bytes
};

/**
 * Unified pipeline layout description generated from shader reflection
 */
struct PipelineLayoutDesc {
  std::vector<DescriptorSetLayoutDesc> setLayouts;
  uint32_t                             pushConstantSize = 0;
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_SHADER_REFLECTION_TYPES_H