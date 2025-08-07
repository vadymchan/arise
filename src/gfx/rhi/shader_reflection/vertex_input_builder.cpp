#include "vertex_input_builder.h"

#include "gfx/rhi/common/rhi_enums.h"
#include "utils/logger/log.h"

#include <algorithm>

namespace arise {
namespace gfx {
namespace rhi {

void VertexInputBuilder::createFromReflection(const std::vector<ShaderVertexInput>&  vertexInputs,
                                              std::vector<VertexInputBindingDesc>&   bindings,
                                              std::vector<VertexInputAttributeDesc>& attributes,
                                              RenderingApi                           api,
                                              uint32_t                               vertexStride,
                                              uint32_t                               instanceStride) {
  bindings.clear();
  attributes.clear();

  if (vertexInputs.empty()) {
    LOG_WARN("No vertex inputs found in shader reflection");
    return;
  }

  bool hasVertexInputs   = false;
  bool hasInstanceInputs = false;

  for (const auto& input : vertexInputs) {
    if (input.semanticName == "INSTANCE") {
      hasInstanceInputs = true;
    } else if (isKnownVertexSemantic_(input.semanticName)) {
      hasVertexInputs = true;
    } else {
      LOG_ERROR("Unknown vertex input semantic: '{}' at location {}", input.semanticName, input.location);
    }
  }

  // vertex binding (binding 0)
  if (hasVertexInputs && vertexStride > 0) {
    VertexInputBindingDesc vertexBinding;
    vertexBinding.binding   = 0;
    vertexBinding.stride    = vertexStride;
    vertexBinding.inputRate = VertexInputRate::Vertex;
    bindings.push_back(vertexBinding);
  }

  // instance binding (binding 1)
  if (hasInstanceInputs && instanceStride > 0) {
    VertexInputBindingDesc instanceBinding;
    instanceBinding.binding   = 1;
    instanceBinding.stride    = instanceStride;
    instanceBinding.inputRate = VertexInputRate::Instance;
    bindings.push_back(instanceBinding);
  }

  // Find the first instance location for offset calculation
  uint32_t firstInstanceLocation = UINT32_MAX;
  for (const auto& input : vertexInputs) {
    if (input.semanticName == "INSTANCE") {
      firstInstanceLocation = std::min(firstInstanceLocation, input.location);
    }
  }

  // Create attributes from reflection data
  for (const auto& input : vertexInputs) {
    if (input.semanticName != "INSTANCE" && !isKnownVertexSemantic_(input.semanticName)) {
      continue;  // Skip unknown semantics
    }

    uint32_t locationsNeeded = calculateLocationsNeeded_(input, api);

    // Create attributes for each location this input spans
    for (uint32_t i = 0; i < locationsNeeded; ++i) {
      VertexInputAttributeDesc attribute;
      attribute.location     = input.location + i;
      attribute.format       = getFormatForLocation_(input, i, api);
      attribute.semanticName = input.semanticName;
      attribute.binding      = (input.semanticName == "INSTANCE") ? 1 : 0;

      // Calculate offset - special handling for instance matrices
      if (input.semanticName == "INSTANCE") {
        uint32_t       matrixRowIndex = input.location - firstInstanceLocation;
        const uint32_t matrixRowSize  = 16;
        attribute.offset              = matrixRowIndex * matrixRowSize;
      } else {
        attribute.offset = getVertexAttributeOffset_(input.semanticName);
      }

      attributes.push_back(attribute);
    }
  }

  // Sort attributes by location for consistency
  std::sort(
      attributes.begin(), attributes.end(), [](const VertexInputAttributeDesc& a, const VertexInputAttributeDesc& b) {
        return a.location < b.location;
      });

  LOG_INFO("Created vertex input layout: {} bindings, {} attributes", bindings.size(), attributes.size());
}

bool VertexInputBuilder::isKnownVertexSemantic_(const std::string& semanticName) {
  return semanticName == "POSITION" || semanticName == "TEXCOORD" || semanticName == "NORMAL"
      || semanticName == "TANGENT" || semanticName == "BITANGENT" || semanticName == "COLOR";
}

uint32_t VertexInputBuilder::getVertexAttributeOffset_(const std::string& semanticName) {
  if (semanticName == "POSITION") {
    return offsetof(ecs::Vertex, position);
  } else if (semanticName == "TEXCOORD") {
    return offsetof(ecs::Vertex, texCoords);
  } else if (semanticName == "NORMAL") {
    return offsetof(ecs::Vertex, normal);
  } else if (semanticName == "TANGENT") {
    return offsetof(ecs::Vertex, tangent);
  } else if (semanticName == "BITANGENT") {
    return offsetof(ecs::Vertex, bitangent);
  } else if (semanticName == "COLOR") {
    return offsetof(ecs::Vertex, color);
  }

  LOG_ERROR(
      "Unknown vertex semantic in offset calculation: {}. This should never happen if isKnownVertexSemantic_ is used "
      "correctly.",
      semanticName);
  return 0;
}

uint32_t VertexInputBuilder::calculateLocationsNeeded_(const ShaderVertexInput& input, RenderingApi api) {
  // Calculate how many locations are needed based on format component count and arraySize

  uint32_t componentsPerLocation = 4;  // vec4
  uint32_t componentCount        = g_getTextureComponentCount(input.format, api);
  uint32_t totalComponents       = componentCount * input.arraySize;

  // Ceiling division to get number of locations needed
  return (totalComponents + componentsPerLocation - 1) / componentsPerLocation;
}

TextureFormat VertexInputBuilder::getFormatForLocation_(const ShaderVertexInput& input,
                                                        uint32_t                 locationIndex,
                                                        RenderingApi             api) {
  bool isFloat = (input.format == TextureFormat::R32f || input.format == TextureFormat::Rg32f
                  || input.format == TextureFormat::Rgb32f || input.format == TextureFormat::Rgba32f);

  uint32_t componentCount           = g_getTextureComponentCount(input.format, api);
  uint32_t totalComponents          = componentCount * input.arraySize;
  uint32_t componentsPerLocation    = 4;
  uint32_t startComponent           = locationIndex * componentsPerLocation;
  uint32_t remainingComponents      = totalComponents - startComponent;
  uint32_t componentsInThisLocation = std::min(remainingComponents, componentsPerLocation);

  if (isFloat) {
    switch (componentsInThisLocation) {
      case 1:
        return TextureFormat::R32f;
      case 2:
        return TextureFormat::Rg32f;
      case 3:
        return TextureFormat::Rgb32f;
      case 4:
      default:
        return TextureFormat::Rgba32f;
    }
  } else {
    LOG_WARN("Non-float vertex input format detected. Defaulting to Rgba32f for multi-location inputs.");
    if (locationIndex == 0 && totalComponents <= 4) {
      return input.format;
    } else {
      return TextureFormat::Rgba32f;
    }
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise