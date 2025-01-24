#pragma once

#include "types.h"
#include <vector>

namespace hlgl {

class Context;

struct ShaderParams {
  // GLSL source code for this shader.
  // One of 'sGlsl', 'sHlsl', or 'pSpv/iSpvSize' is required.
  const char* sGlsl {nullptr};
  // HLSL source code for this shader (currently unimplemented).
  // One of 'sGlsl', 'sHlsl', or 'pSpv/iSpvSize' is required.
  const char* sHlsl {nullptr};
  // Spir-V binary code for this shader.
  // One of 'sGlsl', 'sHlsl', or 'pSpv/iSpvSize' is required.
  const void* pSpv {nullptr};
  // The size, in bytes, of the Spir-V binary code.
  // Required if 'pSpv' is non-null.
  size_t iSpvSize {0};
  // The name of the shader's entry point.
  // Optional, defaults to "main".
  const char* sEntry {"main"};
  // The name of the shader for debugging purposes.
  // Optional.
  const char* sDebugName {nullptr};

  // NOTE: Currently shader source can be provided as either GLSL, HLSL, or SPIRV.
  // As cross-platform concerns become reality in the future, this may have to change.
  // For example, I'd like to support DX12, but afaik there's no way to use GLSL or SPIRV there.
  // If consoles become a concern, we might have to use a non-native shading language...
};

class Shader {
  friend class Pipeline;
  friend class ComputePipeline;
  friend class GraphicsPipeline;
  Shader(const Shader&) = delete;
  Shader& operator = (const Shader&) = delete;
public:
  Shader(Context& context, ShaderParams params);
  ~Shader();
  Shader(Shader&& other) noexcept;
  Shader& operator = (Shader&& other) noexcept;

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

private:
  Context& context_;
  bool initSuccess_ {false};

#if defined HLGL_GRAPHICS_API_VULKAN

  VkShaderModule shader_ {nullptr};
  VkShaderStageFlags stage_ {};
  const char* entry_ {"main"};
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings_ {};
  VkPushConstantRange pushConstants_ {};

#endif // defined HLGL_GRAPHICS_API_x
};

}

