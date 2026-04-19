#ifndef HLGL_VK_SHADER_H
#define HLGL_VK_SHADER_H

#include <hlgl.h>
#include "vulkan-includes.h"
#include <vector>

namespace hlgl {

struct ShaderImpl {
  VkShaderModule module_ {nullptr};
  VkShaderStageFlags stage_ {0};
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings_ {};
  VkPushConstantRange pushConstants_ {};
};

} // namespace hlgl
#endif // HLGL_VK_SHADER_H