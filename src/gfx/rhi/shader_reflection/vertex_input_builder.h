#ifndef ARISE_VERTEX_INPUT_BUILDER_H
#define ARISE_VERTEX_INPUT_BUILDER_H

#include "ecs/components/vertex.h"
#include "gfx/rhi/common/rhi_types.h"
#include "shader_reflection_types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

/**
 * Utility class for automatically creating vertex input layouts from shader reflection
 */
class VertexInputBuilder {
  public:
  /**
   * Create vertex input layout from shader reflection data
   * @param vertexInputs - vertex input variables from shader reflection
   * @param bindings - output vertex input bindings
   * @param attributes - output vertex input attributes
   * @param api - current rendering API
   * @param vertexStride - size of vertex structure (e.g., sizeof(Vertex))
   * @param instanceStride - size of instance structure (e.g., sizeof(Matrix4f))
   */
  static void createFromReflection(const std::vector<ShaderVertexInput>&  vertexInputs,
                                   std::vector<VertexInputBindingDesc>&   bindings,
                                   std::vector<VertexInputAttributeDesc>& attributes,
                                   RenderingApi                           api,
                                   uint32_t                               vertexStride   = 0,
                                   uint32_t                               instanceStride = 0);

  private:
  /**
   * Check if the semantic name corresponds to a known vertex attribute
   * @param semanticName - semantic name to check
   * @return true if semantic is known (POSITION, TEXCOORD, NORMAL, TANGENT, BITANGENT, COLOR)
   */
  static bool isKnownVertexSemantic_(const std::string& semanticName);

  /**
   * Get the offset for a vertex attribute based on semantic name
   * @param semanticName - semantic name (POSITION, TEXCOORD, etc.)
   * @return byte offset within the Vertex structure
   */
  static uint32_t getVertexAttributeOffset_(const std::string& semanticName);

  /**
   * Calculate how many locations a vertex input spans
   */
  static uint32_t calculateLocationsNeeded_(const ShaderVertexInput& input, RenderingApi api);

  /**
   * Get the appropriate format for a specific location within a multi-location input
   * @param locationIndex - index within the spanned locations (0-based)
   */
  static VertexFormat getFormatForLocation_(const ShaderVertexInput& input, uint32_t locationIndex, RenderingApi api);
};

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_VERTEX_INPUT_BUILDER_H