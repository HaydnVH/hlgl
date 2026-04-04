#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "utils/flags.h"

// API-specific definitions and forward declarations.
#if defined HLGL_GRAPHICS_API_VULKAN
#include "vk-fwd.h"
#endif // defined HLGL_GRAPHICS_API_x

namespace hlgl {

// Forward declaration for core types.

// Buffers represent arbitrary memory stored on a GPU.
// This could be in the form of vertices, indices, shader parameters, or anything else really.
class Buffer;

// The Context represents backend functionality managed by the library.
// It tracks and manages global state and enables the creation of other objects.
class Context;

// A Frame takes care of the setup and presentation of a single frame being rendered.
// Operations which can only occur within an active frame are called through a Frame object.
class Frame;

// Pipelines encapsulate a number of shaders and their associated state.
// Pipelines can come in the form of graphics, compute, or raytracing,
// and must be bound before draw or dispatch operations are performed.
class Pipeline;

// Textures represent images stored on a GPU.
// They behave like buffers for the most part in that they could store arbitrary data,
// but have additional functionality for filtering, mipmapping, or use as render targets.
class Texture;

enum class BlendFactor : uint8_t {
  Zero,
  One,
  SrcColor,
  OneMinusSrcColor,
  DstColor,
  OneMinusDstColor,
  SrcAlpha,
  OneMinusSrcAlpha,
  DstAlpha,
  OneMinusDstAlpha,
  ConstColor,
  OneMinusConstColor,
  ConstAlpha,
  OneMinusConstAlpha,
  SrcAlphaSaturate,
  Src1Color,
  OneMinusSrc1Color,
  Src1Alpha,
  OneMinusSrc1Alpha
};

enum class BlendOp : uint8_t {
  Add,
  Subtract,
  SubtractReverse,
  Max,
  Min
};

// A collection of BlendFactors and BlendOp which together define a blending state.
struct BlendSettings {
  BlendFactor srcColorFactor;
  BlendFactor dstColorFactor;
  BlendOp colorOp;
  BlendFactor srcAlphaFactor;
  BlendFactor dstAlphaFactor;
  BlendOp alphaOp;
};

// A predefined BlendSettings appropriate for common additive blending.
constexpr BlendSettings blendAdditive {
  .srcColorFactor = BlendFactor::SrcAlpha,
  .dstColorFactor = BlendFactor::One,
  .colorOp = BlendOp::Add,
  .srcAlphaFactor = BlendFactor::One,
  .dstAlphaFactor = BlendFactor::Zero,
  .alphaOp = BlendOp::Add
};

// A predefined BlendSettings appropriate for common alpha-interpolation blending.
constexpr BlendSettings blendAlpha {
  .srcColorFactor = BlendFactor::SrcAlpha,
  .dstColorFactor = BlendFactor::OneMinusSrcAlpha,
  .colorOp = BlendOp::Add,
  .srcAlphaFactor = BlendFactor::One,
  .dstAlphaFactor = BlendFactor::Zero,
  .alphaOp = BlendOp::Add
};

// A 3-component byte color.
using ColorRGBb = std::array<uint8_t, 3>;
// A 4-component byte color.
using ColorRGBAb = std::array<uint8_t, 4>;
// A 3-component floating-point color.
using ColorRGBf = std::array<float, 3>;
// A 4-component floating-point color.
using ColorRGBAf = std::array<float, 4>;
// A 3-component integer color.
using ColorRGBi = std::array<int32_t, 3>;
// A 4-component integer color.
using ColorRGBAi = std::array<int32_t, 4>;

// Comparison operator primarily used for depth testing.
enum class CompareOp : uint8_t {
  Less,
  Greater,
  Equal,
  LessOrEqual,
  GreaterOrEqual,
  NotEqual,
  Always,
  Never
};

// The faces which should be culled when drawing.
enum class CullMode : uint8_t {
  None,
  Back,
  Front,
  FrontAndBack
};

enum class DebugSeverity {
  Verbose,
  Info,
  Warning,
  Error,
  Fatal,
};
using DebugCallback = std::function<void(DebugSeverity, std::string_view)>;

// A {float, int} pair used for a clear value for a depth-stencil attachment.
struct DepthStencilClearVal { float depth {0.0f}; uint32_t stencil {0}; };

// A device-side pointer to somewhere in VRAM memory.  Used for bindless data.
using DeviceAddress = uint64_t;

// Features which don't need to be supported by a GPU to use HLGL, but may be requested and used by the user.
enum class Feature : uint32_t {
  None                = 0,
  MeshShading         = 1 << 1,
  RayTracing          = 1 << 2,
  Validation          = 1 << 3,
};
using Features = Flags<Feature>;
template <> struct FlagsTraits<Feature> {
  static constexpr bool isFlags {true};
  static constexpr int32_t numBits {4};
};

// Texture filtering when sampled.
enum class FilterMode : uint8_t {
  DontCare,
  Nearest,
  Linear,
  Min,
  Max
};

// Texture format.
enum class Format : uint8_t {
  Undefined = 0,
  RG4i,
  RGBA4i,
  R5G6B5i,
  RGB5A1i,
  R8i,
  RGB8i,
  RGB8i_srgb,
  RGBA8i,
  RGBA8i_srgb,
  BGR8i,
  BGR8i_srgb,
  BGRA8i,
  BGRA8i_srgb,
  R16i,
  RG16i,
  RGB16i,
  RGBA16i,
  R16f,
  RG16f,
  RGB16f,
  RGBA16f,
  R32i,
  RG32i,
  RGB32i,
  RGBA32i,
  R32f,
  RG32f,
  RGB32f,
  RGBA32f,
  A2RGB10i,
  A2BGR10i,
  B10RG11f,
  D24S8,
  D32f,
  D32fS8,
  BC1RGB,
  BC1RGBA,
  BC2,
  BC3,
  BC4,
  BC5,
  BC6,
  BC7
};

// The front or back of a triangle is defined by its winding order.
enum class FrontFace : uint8_t {
  CounterClockwise,
  Clockwise
};

// The type of GPU we're using.
// Order matters! More desirable GPU types should have a greater underlying value.
enum class GpuType : uint8_t {
  Other = 0,
  Cpu,
  Virtual,
  Integrated,
  Discrete
};
inline const char* toStr(GpuType in) {
  switch (in) {
    case GpuType::Other: return "Other";
    case GpuType::Cpu: return "Cpu";
    case GpuType::Virtual: return "Virtual";
    case GpuType::Integrated: return "Integrated";
    case GpuType::Discrete: return "Discrete";
  };
}

// The vendor which produced the GPU being used.
enum class GpuVendor : uint8_t {
  Other,
  AMD,
  ARM,
  ImgTec,
  INTEL,
  NVIDIA,
  Qualcomm
};

// The type of geometry primitive to be drawn.
enum class Primitive : uint8_t {
  Points,
  Lines,
  LineStrip,
  Triangles,
  TriangleStrip,
  TriangleFan,
  LinesWithAdj,
  LineStripWithAdj,
  TrianglesWithAdj,
  TriangleStripWithAdj,
  Patches
};

// The stage for which a shader will be used.
enum class ShaderStage : uint8_t {
  None            = 0,
  Vertex          = 1 << 0,
  Geometry        = 1 << 1,
  TessControl     = 1 << 2,
  TessEvaluation  = 1 << 3,
  Fragment        = 1 << 4,
  Compute         = 1 << 5
};
using ShaderStages = Flags<ShaderStage>;
template <> struct FlagsTraits<ShaderStage> {
  static constexpr bool isFlags {true};
  static constexpr int32_t numBits {6};
};

// A simple {major, minor, patch} tuple for passing around versions of things.
struct Version {
  uint32_t major {0}, minor {0}, patch {0};
};

struct Viewport {
  int32_t x {0}, y {0};
  uint32_t w {0}, h {0};
};

enum class VsyncMode : uint8_t {
  Disabled,
  Fifo,
  FifoRelaxed,
  Mailbox
};
constexpr inline const char* toStr(VsyncMode mode) {
  switch (mode) {
    case VsyncMode::Disabled: return "Disabled";
    case VsyncMode::Fifo: return "Fifo";
    case VsyncMode::FifoRelaxed: return "FifoRelaxed";
    case VsyncMode::Mailbox: return "Mailbox";
  };
}

// WrapMode defines how texture addressing should handle values beyond the [0,1] range.
enum class WrapMode {
  DontCare,
  Repeat,
  MirrorRepeat,
  ClampToEdge,
  ClampToBorder,
  MirrorClampToEdge,
};

} // namespace hlgl

// The definition of WindowHandle is platform-dependent.
// Forward-declaring structs here lets us avoid including potentially problematic system headers (lookin' at you, windows.h).

#if defined HLGL_WINDOW_LIBRARY_GLFW
struct GLFWwindow;
namespace hlgl { using WindowHandle = GLFWwindow*; }
#elif defined HLGL_WINDOW_LIBRARY_SDL
struct SDL_Window;
namespace hlgl { using WindowHandle = SDL_Window*; }
#elif defined HLGL_WINDOW_LIBARY_NATIVE_WIN32
namespace hlgl { using WindowHandle = void*; }
#endif
