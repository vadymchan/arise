#ifndef ARISE_SHADER_REFLECTION_TYPES_H
#define ARISE_SHADER_REFLECTION_TYPES_H

#include "gfx/rhi/common/rhi_types.h"

#include <string>
#include <unordered_map>
#include <vector>

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
  uint32_t     location     = 0;                      // input location (Vulkan) / semantic index (DirectX)
  std::string  semanticName = "";                     // semantic name (DirectX) / inferred from location (Vulkan)
  VertexFormat format       = VertexFormat::Rgba32f;  // inferred format
  uint32_t     arraySize    = 1;                      // array size (1 if not array)
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

//------------------------------------------------------
// Vertex attribute semantic name to location mapping
//------------------------------------------------------

/**
 * Static lookup table for vertex attribute semantic names to locations.
 * This table must match the location assignments in shader_semantics.hlsli
 */
inline const std::unordered_map<std::string, uint32_t> kVertexSemanticToLocation = {
  {        "POSITION",  0},
  {        "TEXCOORD",  1},
  {          "NORMAL",  2},
  {         "TANGENT",  3},
  {       "BITANGENT",  4},
  {           "COLOR",  5},
  {        "INSTANCE",  6},
 // INSTANCE occupies 6..9, so custom attributes start at 10
  {"LOCAL_MIN_CORNER", 10},
  {"LOCAL_MAX_CORNER", 11},
  {"WORLD_MIN_CORNER", 12},
  {"WORLD_MAX_CORNER", 13}
};

/**
 * Get location for a given semantic name
 * @param semanticName The semantic name to look up
 * @return The location index, or UINT32_MAX if not found
 */
inline uint32_t getLocationForSemantic(const std::string& semanticName) {
  auto it = kVertexSemanticToLocation.find(semanticName);
  return (it != kVertexSemanticToLocation.end()) ? it->second : UINT32_MAX;
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_SHADER_REFLECTION_TYPES_H