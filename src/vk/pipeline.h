#ifndef HLGL_VK_PIPELINE_H
#define HLGL_VK_PIPELINE_H

#include <hlgl.h>
#include "vulkan-headers.h"
#include "../utils/array.h"

namespace hlgl {

struct PipelineImpl {
  PipelineImpl(Pipeline::ComputeParams&& params);
  PipelineImpl(Pipeline::GraphicsParams&& params);

  VkPipeline pipeline {nullptr};
  VkPipelineLayout layout {nullptr};
  VkPushConstantRange pushConstRange {};
  VkPipelineBindPoint bindPoint {};
  bool isOpaque {true};
  
  bool initLayout(const hlgl::Array<hlgl::ShaderInfo, 8>& shaders, VkShaderStageFlags stages);
};

} // namespace hlgl
#endif // HLGL_VK_PIPELINE_H