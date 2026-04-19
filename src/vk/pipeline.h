#ifndef HLGL_VK_PIPELINE_H
#define HLGL_VK_PIPELINE_H

#include <hlgl.h>
#include "vulkan-includes.h"
#include "../utils/array.h"

namespace hlgl {

struct PipelineImpl {
  VkPipeline pipeline_ {nullptr};
  VkPipelineLayout layout_ {nullptr};
  VkPushConstantRange pushConstRange_ {};
  VkPipelineBindPoint bindPoint_ {};
  bool isOpaque_ {true};
  
  bool initLayout(const hlgl::Array<hlgl::ShaderInfo, 8>& shaders, VkShaderStageFlags stages);
};

} // namespace hlgl
#endif // HLGL_VK_PIPELINE_H