#pragma once

#include <hlgl/types.h>
#include <hlgl/pipeline.h>
#include <vector>

namespace hlgl {

class Shader {
  friend class Pipeline;
  friend class ComputePipeline;
  friend class GraphicsPipeline;
  Shader(const Shader&) = delete;
  Shader& operator = (const Shader&) = delete;
public:
  Shader(ShaderParams params);
  ~Shader();
  Shader(Shader&& other) noexcept = default;
  Shader& operator = (Shader&& other) noexcept = default;

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

private:
  bool initSuccess_ {false};

#if defined HLGL_GRAPHICS_API_VULKAN

  VkShaderModule shader_ {nullptr};
  VkShaderStageFlags stage_ {};
  const char* entry_ {"main"};
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings_ {};
  VkPushConstantRange pushConstants_ {};

#endif // defined HLGL_GRAPHICS_API_x
};

} // namespace hlgl

