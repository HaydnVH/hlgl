#ifndef HLGL_VK_SHADER_H
#define HLGL_VK_SHADER_H

#include <hlgl.h>
#include "vulkan-includes.h"
#include <vector>

namespace hlgl {

class Shader {
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;
public:
  Shader(Shader&&) noexcept = default;
  Shader& operator=(Shader&&) noexcept = default;

  Shader(CreateShaderParams&& params);
  ~Shader();

  bool isValid() const { return (module_); }

  VkShaderModule getModule() { return module_; }
  VkShaderStageFlags getStages() const { return stage_; }
  VkPushConstantRange getPushConstants() const { return pushConstants_; }


private:
  VkShaderModule module_ {nullptr};
  VkShaderStageFlags stage_ {0};
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings_ {};
  VkPushConstantRange pushConstants_ {};
};

} // namespace hlgl
#endif // HLGL_VK_SHADER_H