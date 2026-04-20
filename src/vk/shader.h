#ifndef HLGL_VK_SHADER_H
#define HLGL_VK_SHADER_H

#include <hlgl.h>
#include "vulkan-includes.h"
#include <vector>

namespace hlgl {

struct ShaderImpl {
  ShaderImpl(Shader::CreateParams&& params);

  VkShaderModule module {nullptr};
  VkShaderStageFlags stage {0};
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings {};
  VkPushConstantRange pushConstants {};
};

} // namespace hlgl
#endif // HLGL_VK_SHADER_H