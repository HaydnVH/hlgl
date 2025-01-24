#pragma once

#include "types.h"
#include "shader.h"
#include <vector>

namespace hlgl {

class Context;

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

struct ComputePipelineParams {
  Shader* shader {nullptr};       // Compute Shader information and source.  Required!
  const char* sDebugName {nullptr};      // Name used for debugging.  Optional.
};

struct GraphicsPipelineParams {
  // The collection of shaders executed on this graphics pipeline.
  // Fragment and either Vertex or Mesh shaders are required; all others are optional.
  std::initializer_list<Shader*> shaders;

  Primitive ePrimitive {Primitive::Triangles};                    // The type of primitives drawn by this pipeline.  Optional, defaults to Triangles.
  bool bPrimitiveRestart {false};                                 // Whether to enable primitive restart for strip-based primitives.  Optional, defaults to false.
  CullMode eCullMode {CullMode::Back};                            // Which faces to cull based on winding.  Optional, defaults to backface culling.
  FrontFace eFrontFace {FrontFace::CounterClockwise};             // Which face is considered "front" based on winding.  Optional, defaults to counter-clockwise.
  uint32_t iMsaa {1};                                             // Number of samples to use for MSAA.  Optional, defaults to 1 which disables MSAA.

  std::optional<DepthAttachment> depthAttachment {std::nullopt};  // Format and settings related to the depth-stencil buffer.  Optional, defaults to std::nullopt which disables z-buffering.
  std::initializer_list<ColorAttachment> colorAttachments;        // List of formats and blend states for each color attachment.  At least one color attachment is required.
  const char* sDebugName {nullptr};                               // Name used for debugging.  Optional.
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
  Pipeline(Context& context): context_(context) {}
  Context& context_;
  bool initSuccess_ {false};

  bool initShaders(const std::initializer_list<Shader*>& shaders);

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
  ComputePipeline(Context& context, ComputePipelineParams params);
};

class GraphicsPipeline: public Pipeline {
  friend class Frame;
public:
  GraphicsPipeline(Context& context, GraphicsPipelineParams params);
};

} // namespace hlgl