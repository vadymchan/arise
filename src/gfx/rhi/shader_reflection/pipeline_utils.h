#ifndef ARISE_PIPELINE_UTILS_H
#define ARISE_PIPELINE_UTILS_H

#include "shader_reflection_types.h"

#include <memory>
#include <vector>

namespace arise {
namespace gfx {
namespace rhi {

class Device;
class Shader;
class DescriptorSetLayout;

namespace pipeline_utils {

PipelineLayoutDesc generatePipelineLayoutFromShaders(const std::vector<Shader*>& shaders);

std::vector<const DescriptorSetLayout*> createDescriptorSetLayouts(
    Device*                                            device,
    const PipelineLayoutDesc&                          pipelineLayout,
    std::vector<std::unique_ptr<DescriptorSetLayout>>& ownedLayouts);

}  // namespace pipeline_utils

}  // namespace rhi
}  // namespace gfx
}  // namespace arise

#endif  // ARISE_PIPELINE_UTILS_H