#pragma once

#include "wcgl-types.h"
#include "wcgl-context.h"
#include <vector>

namespace wcgl {

class Context;

struct ShaderParams {
  // The name of the shader, used for caching and debugging.
  // Required!
  const char* sName {nullptr};
  // The name of the shader's entry point.
  // Optional, defaults to "main".
  const char* sEntry {"main"};
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

  // NOTE: Currently shader source can be provided as either GLSL, HLSL, or SPIRV.
  // As cross-platform concerns become reality in the future, this may have to change.
  // For example, I'd like to support DX12, but afaik there's no way to use GLSL or SPIRV there.
  // If consoles become a concern, we might have to use a non-native shading language...
};

struct ColorAttachment {
  // The expected format for this pipeline color attachment.
  // Required!
  Format format {Format::Undefined};
  // The blending settings for this color attachment.
  // Optional, defaults to std::nullopt which disables blending.
  std::optional<BlendSettings> blend = std::nullopt;
};

struct DepthAttachment {
  // The expected format for this pipeline depth attachment.
  // Required!
  Format format {Format::Undefined};
  // Whether or not depth testing should be enabled.
  // Optional, defaults to true.
  bool bTest {true};
  // Whether or not depth writing should be enabled.
  // Optional, defaults to true.
  bool bWrite {true};
  // Which comparison operator to use for depth testing.
  // Optional, defaults to Greater (pixels with greater depth are drawn over pixels with lesser depth).
  CompareOp eCompare {CompareOp::Greater};
  struct Bias {
    float fConst {0.0f};
    float fClamp {0.0f};
    float fSlope {0.0f};
  };
  // Depth bias settings, used for things like shadow mapping.
  // Optional, defaults to std::nullopt which disables depth bias.
  std::optional<Bias> bias {std::nullopt};
};

struct PipelineParams {

  // The type of primitives drawn by this graphics pipeline.
  // Optional, defaults to Triangles.
  Primitive ePrimitive {Primitive::Triangles};
  // Whether to enable primitive restart for strips.
  // Optional, defaults to false.
  bool bPrimitiveRestart {false};

  // Which faces to cull based on winding; back, front, both, or neither.
  // Optional, defaults to back.
  CullMode eCullMode {CullMode::Back};
  // Which face is considered "front" based on winding.
  // Optional, defaults to counter-clockwise.
  FrontFace eFrontFace {FrontFace::CounterClockwise};

  // Amount of samples to use for MSAA.
  // Optional, defaults to 1 which disables MSAA.
  uint32_t iMsaa {1};

  // Format and settings related to the z-buffer.
  // Optional, defaults to std::nullopt which disables z-buffering.
  std::optional<DepthAttachment> depthAttachment {std::nullopt};

  // List of formats and blend states for each color attachment.
  // Required! At least one color attachment must be present for the fragment shader to output anything!
  std::initializer_list<ColorAttachment> colorAttachments;

  // The list of shaders to be used by this pipeline.
  // If a compute shader is present, this will be a compute pipeline, and no other shader stages should be present.
  // Otherwise, this will be a graphics pipeline, and must contain a fragment shader in addition to a vertex or mesh shader.
  std::initializer_list<ShaderParams> shaders;

  // TODO: Implement a hash function to turn a CreatePipelineParams into a searchable integer.
  // This would let us easily cache and retreive pipelines.
};

class Pipeline {
  friend class Frame;

  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

public:
  Pipeline(Pipeline&&) = default;
  Pipeline& operator=(Pipeline&&) = default;

  Pipeline(const Context& context, PipelineParams params);
  ~Pipeline();

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

protected:
  const Context& context_;
  bool initSuccess_ {false};

#if defined WCGL_GRAPHICS_API_VULKAN

  VkPipeline pipeline_ {nullptr};
  VkPipelineLayout layout_ {nullptr};
  VkDescriptorSetLayout descLayout_ {nullptr};
  std::vector<VkDescriptorType> descTypes_ {};
  VkPushConstantRange pushConstRange_ {};
  VkPipelineBindPoint type_ {};

#endif // defined WCGL_GRAPHICS_API_x
};

} // namespace wcgl