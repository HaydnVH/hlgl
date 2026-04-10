#pragma once

#include "types.h"
#include <array>
#include <vector>

namespace hlgl {

struct ColorAttachmentParams {
  // The expected format for this pipeline color attachment.
  // Required!
  Format format {Format::Undefined};
  // The blending settings for this color attachment.
  // Optional, defaults to std::nullopt which disables blending.
  std::optional<BlendSettings> blend {std::nullopt};
};

struct DepthAttachmentParams {
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
  // Optional, defaults to Less (pixels with lesser depth are drawn over pixels with greater depth).
  CompareOp eCompare {CompareOp::LessOrEqual};
  struct Bias {
    float fConst {0.0f};
    float fClamp {0.0f};
    float fSlope {0.0f};
  };
  // Depth bias settings, used for things like shadow mapping.
  // Optional, defaults to std::nullopt which disables depth bias.
  std::optional<Bias> bias {std::nullopt};
};


enum class ShaderSrcType {
  Undefined = 0,
  GLSL,
  HLSL,
  Slang
};

struct ShaderParams {
  // The source code for this shader.  The shader will be compiled to Spir-V and then used to create the pipeline.
  const char* src {nullptr};

  // The source language that a shader's source is provided in.
  // If "undefined", the compiler may be able to detect it.
  ShaderSrcType srcType {ShaderSrcType::Undefined};

  // The shader can also be provided as precompiled Spir-V bytecode.  This is much faster than compiling at load time.
  const void* spvData {nullptr};
  size_t spvSize {0};

  // The entrypoint for this shader (defaults to "main").
  const char* entry {"main"};

  // The debug name for this shader (optional).
  const char* debugName {nullptr};

  // Returns true if this shader param is valid (either source or precompiled Spir-V is provided).
  operator bool() const { return (src || spvData); }
};

struct ComputePipelineParams {
  ShaderParams compShader {};  // Compute shader (required)
  const char* sDebugName {nullptr};      // Name used for debugging (optional)
};

struct GraphicsPipelineParams {

  ShaderParams vertShader {}; // Vertex shader
  ShaderParams geomShader {}; // Geometry shader
  ShaderParams tescShader {}; // Tesselation Control shader
  ShaderParams teseShader {}; // Tesselation Evaluation shader
  ShaderParams fragShader {}; // Fragment shader (required)

  ShaderParams taskShader {}; // Task shader
  ShaderParams meshShader {}; // Mesh shader

  Primitive primitive {Primitive::Triangles};                    // The type of primitives drawn by this pipeline.  Optional, defaults to Triangles.
  bool primitiveRestart {false};                                 // Whether to enable primitive restart for strip-based primitives.  Optional, defaults to false.
  CullMode cullMode {CullMode::Back};                            // Which faces to cull based on winding.  Optional, defaults to backface culling.
  FrontFace frontFace {FrontFace::CounterClockwise};             // Which face is considered "front" based on winding.  Optional, defaults to counter-clockwise.
  uint32_t msaa {1};                                             // Number of samples to use for MSAA.  Optional, defaults to 1 which disables MSAA.

  std::initializer_list<ColorAttachmentParams> colorAttachments;        // List of formats and blend states for each color attachment.  At least one color attachment is required.
  std::optional<DepthAttachmentParams> depthAttachment {std::nullopt};  // Format and settings related to the depth-stencil buffer.  Optional, defaults to std::nullopt which disables z-buffering.
  const char* debugName {nullptr};                               // Name used for debugging.  Optional.
};

struct RaytracingPipelineParams {
  ShaderParams rgenShader {}; // Ray Generation shader
  ShaderParams isecShader {}; // Intersection shader
  ShaderParams ahitShader {}; // Any Hit shader
  ShaderParams chitShader {}; // Closest Hit shader
  ShaderParams missShader {}; // Miss shader

  // TODO: Actually implement raytracing support.
};

class Shader;

class Pipeline {
  friend class Frame;

  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

public:
  Pipeline(Pipeline&&) = delete;
  Pipeline& operator=(Pipeline&&) = delete;
  ~Pipeline();

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

  bool isOpaque() const { return isOpaque_; }

protected:
  Pipeline() {}
  bool initSuccess_ {false};

  bool isOpaque_ {true};

  bool initShaders(const std::vector<Shader>& shaders);

#if defined HLGL_GRAPHICS_API_VULKAN

  VkPipeline pipeline_ {nullptr};
  VkPipelineLayout layout_ {nullptr};
  VkDescriptorSetLayout descLayout_ {nullptr};
  std::vector<VkDescriptorType> descTypes_ {};
  VkPushConstantRange pushConstRange_ {};
  VkPipelineBindPoint type_ {};

#endif // defined HLGL_GRAPHICS_API_x
};

class ComputePipeline : public Pipeline {
  friend class Frame;
public:
  ComputePipeline(ComputePipelineParams params);
};

class GraphicsPipeline: public Pipeline {
  friend class Frame;
public:
  GraphicsPipeline(GraphicsPipelineParams params);
};

} // namespace hlgl