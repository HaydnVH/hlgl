#pragma once

#include "types.h"
#include "context.h"
#include <vector>

namespace hlgl {

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
  std::optional<BlendSettings> blend {std::nullopt};
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

struct ComputePipelineParams {
  ShaderParams computeShader;          // Compute Shader information and source.  Required!
};

struct GraphicsPipelineParams {
  ShaderParams vertexShader {};        // Vertex Shader information and source.  Optional, but a vertex shader or mesh shader must be present.
  ShaderParams tessCtrlShader {};      // Tessellation Control Shader information and source.  Optional.
  ShaderParams tessEvalShader {};      // Tessellation Evaluation Shader information and source.  Optional.
  ShaderParams geometryShader {};      // Geometry Shader information and source.  Optional.
  ShaderParams taskShader {};          // Task Shader information and source.  Optional.
  ShaderParams meshShader {};          // Mesh Shader information and source.  Optional, but a vertex shader or mesh shader must be present.
  ShaderParams fragmentShader;         // Fragment Shader information and source.  Required!

  Primitive ePrimitive {Primitive::Triangles};                    // The type of primitives drawn by this pipeline.  Optional, defaults to Triangles.
  bool bPrimitiveRestart {false};                                 // Whether to enable primitive restart for strip-based primitives.  Optional, defaults to false.
  CullMode eCullMode {CullMode::Back};                            // Which faces to cull based on winding.  Optional, defaults to backface culling.
  FrontFace eFrontFace {FrontFace::CounterClockwise};             // Which face is considered "front" based on winding.  Optional, defaults to counter-clockwise.
  uint32_t iMsaa {1};                                             // Number of samples to use for MSAA.  Optional, defaults to 1 which disables MSAA.

  std::optional<DepthAttachment> depthAttachment {std::nullopt};  // Format and settings related to the depth-stencil buffer.  Optional, defaults to std::nullopt which disables z-buffering.
  std::initializer_list<ColorAttachment> colorAttachments;        // List of formats and blend states for each color attachment.  At least one color attachment is required.
};

struct RaytracingPipelineParams {
  ShaderParams rayGenShader {};        // Ray Generation Shader information and source.
  ShaderParams intersectionShader {};  // Intersection Shader information and source.
  ShaderParams anyHitShader {};        // Any Hit Shader information and source.
  ShaderParams closestHitShader {};    // Closest Hit Shader information and source.
  ShaderParams missShader {};          // Miss Shader information and source.

  // TODO: Actually implement raytracing support.
};

class Pipeline {
  friend class Frame;

  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

public:
  Pipeline(Pipeline&&) = default;
  Pipeline& operator=(Pipeline&&) = default;
  ~Pipeline();

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

protected:
  Pipeline(const Context& context): context_(context) {}
  const Context& context_;
  bool initSuccess_ {false};

  class ShaderModule;
  std::vector<ShaderModule> initShaders(const std::initializer_list<ShaderParams>& shaderParams);

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
  ComputePipeline(const Context& context, ComputePipelineParams params);
};

class GraphicsPipeline: public Pipeline {
  friend class Frame;
public:
  GraphicsPipeline(const Context& context, GraphicsPipelineParams params);
};

} // namespace hlgl