#include "gfx/rhi/backends/vulkan/shader_vk.h"

#include "gfx/rhi/backends/vulkan/device_vk.h"
#include "utils/logger/log.h"

namespace arise {
namespace gfx {
namespace rhi {

ShaderVk::ShaderVk(const ShaderDesc& desc, DeviceVk* device)
    : Shader(desc)
    , m_device_(device)
    , m_shaderModule_(VK_NULL_HANDLE) {
  initialize(desc.code);
}

ShaderVk::~ShaderVk() {
  release();
}

void ShaderVk::initialize(const std::vector<uint8_t>& code) {
  std::lock_guard<std::mutex> lock(m_mutex_);

  m_desc_.code = code;

  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize                 = code.size();
  createInfo.pCode                    = reinterpret_cast<const uint32_t*>(code.data());

  if (vkCreateShaderModule(m_device_->getDevice(), &createInfo, nullptr, &m_shaderModule_) != VK_SUCCESS) {
    LOG_ERROR("Failed to create Vulkan shader module");
    return;
  }
}

void ShaderVk::release() {
  std::lock_guard<std::mutex> lock(m_mutex_);

  if (m_shaderModule_ != VK_NULL_HANDLE) {
    vkDestroyShaderModule(m_device_->getDevice(), m_shaderModule_, nullptr);
    m_shaderModule_ = VK_NULL_HANDLE;
  }
}

}  // namespace rhi
}  // namespace gfx
}  // namespace arise