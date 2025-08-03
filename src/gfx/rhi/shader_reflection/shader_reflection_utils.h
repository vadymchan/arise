#ifndef ARISE_SHADER_REFLECTION_UTILS_H
#define ARISE_SHADER_REFLECTION_UTILS_H

#include "gfx/rhi/common/rhi_types.h"
#include "shader_reflection_types.h"

namespace arise {
namespace gfx {
namespace rhi {
namespace reflection_utils {

void logPipelineLayoutContents(const PipelineLayoutDesc& layout);

const char* bindingTypeToString(ShaderBindingType type);

}  // namespace reflection_utils
}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_SHADER_REFLECTION_UTILS_H