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

  auto isInstanceLikeSemantic = [](const std::string& name) {
    return name == "INSTANCE" || name == "LOCAL_MIN_CORNER" || name == "LOCAL_MAX_CORNER" || name == "WORLD_MIN_CORNER"
        || name == "WORLD_MAX_CORNER";
  };

  for (const auto& input : vertexInputs) {
    if (isInstanceLikeSemantic(input.semanticName)) {
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

  // Pre-compute offsets for extra instance attributes (custom semantics), ordered by location
  struct InstanceExtraDesc {
    uint32_t     location;
    std::string  semantic;
    VertexFormat format;
  };
  std::vector<InstanceExtraDesc> instanceExtras;
  instanceExtras.reserve(vertexInputs.size());
  for (const auto& input : vertexInputs) {
    if (input.semanticName != "INSTANCE" && isInstanceLikeSemantic(input.semanticName)) {
      instanceExtras.push_back({input.location, input.semanticName, input.format});
    }
  }
  std::sort(instanceExtras.begin(), instanceExtras.end(), [](const InstanceExtraDesc& a, const InstanceExtraDesc& b) {
    return a.location < b.location;
  });

  auto getFormatSizeBytes = [api](VertexFormat fmt) -> uint32_t {
    switch (fmt) {
      case VertexFormat::R8:
      case VertexFormat::R8ui:
        return 1;
      case VertexFormat::R16f:
        return 2;
      case VertexFormat::R32f:
      case VertexFormat::R32ui:
      case VertexFormat::R32si:
        return 4;

      case VertexFormat::Rg8:
        return 2;
      case VertexFormat::Rg16f:
        return 4;
      case VertexFormat::Rg32f:
        return 8;

      case VertexFormat::Rgb8:
        return 3;
      case VertexFormat::Rgb16f:
        return 6;
      case VertexFormat::Rgb32f:
        return 12;

      case VertexFormat::Rgba8:
      case VertexFormat::Rgba8ui:
      case VertexFormat::Rgba8si:
        return 4;
      case VertexFormat::Rgba16f:
        return 8;
      case VertexFormat::Rgba32f:
        return 16;

      case VertexFormat::Count:
      default:
        LOG_WARN("Unknown VertexFormat in getFormatSizeBytes, defaulting to 16 bytes");
        return 16;
    }
  };

  std::unordered_map<uint32_t, uint32_t> instanceExtraOffsetByLocation;         // key: location, value: byte offset
  {
    uint32_t runningOffset = (firstInstanceLocation != UINT32_MAX) ? 64u : 0u;  // 4 vec4 rows of the instance matrix
    for (const auto& extra : instanceExtras) {
      instanceExtraOffsetByLocation[extra.location]  = runningOffset;
      runningOffset                                 += getFormatSizeBytes(extra.format);
    }
  }

  // Create attributes from reflection data
  for (const auto& input : vertexInputs) {
    if (!isInstanceLikeSemantic(input.semanticName) && !isKnownVertexSemantic_(input.semanticName)) {
      continue;  // Skip unknown semantics
    }

    uint32_t locationsNeeded = calculateLocationsNeeded_(input, api);

    // Create attributes for each location this input spans
    for (uint32_t i = 0; i < locationsNeeded; ++i) {
      VertexInputAttributeDesc attribute;
      attribute.location     = input.location + i;
      attribute.format       = getFormatForLocation_(input, i, api);
      attribute.semanticName = input.semanticName;
      attribute.binding      = (isInstanceLikeSemantic(input.semanticName)) ? 1 : 0;

      // Calculate offset - special handling for instance matrices
      if (input.semanticName == "INSTANCE") {
        uint32_t       matrixRowIndex = input.location - firstInstanceLocation;
        const uint32_t matrixRowSize  = 16;
        attribute.offset              = matrixRowIndex * matrixRowSize;
      } else if (isInstanceLikeSemantic(input.semanticName)) {
        auto it          = instanceExtraOffsetByLocation.find(input.location);
        attribute.offset = (it != instanceExtraOffsetByLocation.end()) ? it->second : 0u;
      } else {
        attribute.offset = getVertexAttributeOffset_(input.semanticName);
      }

      attributes.push_back(attribute);
    }
  }

  // Sort for consistency
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
  uint32_t componentCount        = g_getVertexFormatComponentCount(input.format);
  uint32_t totalComponents       = componentCount * input.arraySize;

  // Ceiling division to get number of locations needed
  return (totalComponents + componentsPerLocation - 1) / componentsPerLocation;
}

VertexFormat VertexInputBuilder::getFormatForLocation_(const ShaderVertexInput& input,
                                                       uint32_t                 locationIndex,
                                                       RenderingApi             api) {
  bool isFloat = (input.format == VertexFormat::R32f || input.format == VertexFormat::Rg32f
                  || input.format == VertexFormat::Rgb32f || input.format == VertexFormat::Rgba32f);

  uint32_t componentCount           = g_getVertexFormatComponentCount(input.format);
  uint32_t totalComponents          = componentCount * input.arraySize;
  uint32_t componentsPerLocation    = 4;
  uint32_t startComponent           = locationIndex * componentsPerLocation;
  uint32_t remainingComponents      = totalComponents - startComponent;
  uint32_t componentsInThisLocation = std::min(remainingComponents, componentsPerLocation);

  if (isFloat) {
    switch (componentsInThisLocation) {
      case 1:
        return VertexFormat::R32f;
      case 2:
        return VertexFormat::Rg32f;
      case 3:
        return VertexFormat::Rgb32f;
      case 4:
      default:
        return VertexFormat::Rgba32f;
    }
  } else {
    LOG_WARN("Non-float vertex input format detected. Defaulting to Rgba32f for multi-location inputs.");
    if (locationIndex == 0 && totalComponents <= 4) {
      return input.format;
    } else {
      return VertexFormat::Rgba32f;
    }
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise