#ifndef HLGL_PIPELINE_H
#define HLGL_PIPELINE_H

#include "hlgl-base.h"

namespace hlgl {

  // When a graphics pipeline is created, it needs to know about the color attachments it'll be rendering to.
struct ColorAttachmentInfo {
  ImageFormat format {ImageFormat::Undefined};          // The expected format for this color attachment.  Required!
  std::optional<BlendSettings> blending {std::nullopt}; // Blend settings for this color attachment.  Optional, defaults to nullopt which disables blending.
};

// When a graphics pipeline is created, it needs to know about the depth-stencil attachment it'll be using.
struct DepthAttachmentInfo {
  ImageFormat format {ImageFormat::Undefined}; // The expected format for this depth attachment.  Required!
  bool test {true};  // Whether or not depth testing should be enabled.  Optional, defaults to true.
  bool write {true};  // Whether or not depth writing should be enabled.  Optional, defaults to true.
  CompareOp compare {CompareOp::LessOrEqual}; // Which comparison operator to use for depth testing.  Optional, defaults to less (pixels with lesser depth are drawn over pixels with greater depth).
  struct Bias {
    float constFactor {0.0f};   // Scalar factor controlling the constant depth value added to each fragment.
    float clamp {0.0f};         // The maximum (or minimum) depth bias of a fragment.
    float slopeFactor {0.0f};   // Scalar factor applied to a fragment's slope in depth bias calculations.
  };
  std::optional<Bias> bias {std::nullopt};  // Sets the depth bias and clamp for the pipeline.  Optional, defaults to nullopt which disables depth bias.
};

struct PipelineImpl;

// A pipeline represents one or more shaders and any associated state required to execute the shaders.
class Pipeline {
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;
  public:
  Pipeline(Pipeline&&) noexcept = default;
  Pipeline& operator=(Pipeline&&) noexcept = default;
  ~Pipeline();

  struct ComputeParams {
    ShaderInfo compShader;  // Compute shader
    const char* debugName {nullptr};
    };
  Pipeline(ComputeParams params);

  struct GraphicsParams {
    ShaderInfo vertShader;                                            // Vertex shader, either this or mesh shader is required.
    ShaderInfo geomShader;                                            // Geometry shader, optional.
    ShaderInfo tescShader;                                            // Tesselation Control shader, optional.
    ShaderInfo teseShader;                                            // Tesselation Evaluation shader, optional.
    ShaderInfo fragShader;                                            // Fragment shader, required!
    ShaderInfo taskShader;                                            // Task shader, optional.
    ShaderInfo meshShader;                                            // Mesh shader, either this or vertex shader is required.

    Primitive primitive {Primitive::Triangles};                       // The type of primitives drawn by this pipeline.  Defaults to Triangles.
    bool primitiveRestart {false};                                    // Whether to enable primitive restart for strip-based primitives.  Defaults to false.
    CullMode cullMode {CullMode::Back};                               // Which fases to cull based on winding.  Defaults to backface culling.
    FrontFace frontFace {FrontFace::CounterClockwise};                // Which winding order to consider "front".  Defaults to counter-clockwise.
    uint32_t msaa {1};                                                // Number of samples to use for MSAA.  Defaults to 1 which disables MSAA.

    std::initializer_list<ColorAttachmentInfo> colorAttachments;      // Descriptions for each color attachment that this pipeline will render to.
    std::optional<DepthAttachmentInfo> depthAttachment {std::nullopt};// Description of the depth buffer used by this pipeline and how to handle depth buffering.  Optional, defaults to nullopt which disables depth buffering.
    const char* debugName {nullptr};
    };
  Pipeline(GraphicsParams params);

  bool isValid() const { return (bool)_pimpl; }
  operator bool() const { return (bool)_pimpl; }

  bool isCompute() const;
  bool isGraphics() const;
  bool isOpaque() const;

  std::unique_ptr<PipelineImpl> _pimpl;
};

} // namespace hlgl
#endif // HLGL_PIPELINE_H