#ifndef HLGL_VK_PIPELINE_H
#define HLGL_VK_PIPELINE_H

#include <hlgl.h>
#include "vulkan-includes.h"
#include "../utils/array.h"

namespace hlgl {

class Pipeline {
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;
public:
  Pipeline(Pipeline&&) noexcept = default;
  Pipeline& operator=(Pipeline&&) noexcept = default;

  Pipeline(CreateComputePipelineParams&& params);
  Pipeline(CreateGraphicsPipelineParams&& params);
  ~Pipeline();

  bool isValid() { return (pipeline_ && layout_); }

  VkPipeline getPipeline() { return pipeline_; }
  VkPipelineLayout getLayout() const  { return layout_; }
  VkPushConstantRange getPushConstRange() const { return pushConstRange_; }
  VkPipelineBindPoint getBindPoint() const { return bindPoint_; }

private:
  bool initLayout(const hlgl::Array<hlgl::ShaderInfo, 8>& shaders, VkShaderStageFlags stages);

  VkPipeline pipeline_ {nullptr};
  VkPipelineLayout layout_ {nullptr};
  VkPushConstantRange pushConstRange_ {};
  VkPipelineBindPoint bindPoint_ {};
  bool isOpaque_ {true};
};

} // namespace hlgl
#endif // HLGL_VK_PIPELINE_H