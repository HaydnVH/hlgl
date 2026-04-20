#ifndef HLGL_VK_SHADER_H
#define HLGL_VK_SHADER_H

#include <hlgl.h>
#include "vulkan-headers.h"
#include <vector>

namespace hlgl {

struct ShaderImpl {
  ShaderImpl(Shader::CreateParams&& params);

  VkShaderModule module {nullptr};
};

} // namespace hlgl
#endif // HLGL_VK_SHADER_H