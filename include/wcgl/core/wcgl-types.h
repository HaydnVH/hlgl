#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

// API-specific definitions and forward declarations.
#if defined WCGL_GRAPHICS_API_VULKAN
#include "wcgl-vk-fwd.h"
#endif // defined WCGL_GRAPHICS_API_x

namespace wcgl {

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

// Helper struct for managing flags for custom bitfield enums.

template <typename BitT>
struct Flags {
public:
  using MaskT = std::underlying_type_t<BitT>;

  constexpr Flags(): mask_(0) {}
  constexpr Flags(BitT bit): mask_(static_cast<MaskT>(bit)) {}
  constexpr Flags(const Flags<BitT>& other) = default;
  constexpr Flags(MaskT mask): mask_(mask) {}

  constexpr auto operator<=>(const Flags<BitT>&) const = default;
  constexpr bool operator!() const { return !mask_; }
  constexpr Flags<BitT> operator&(const Flags<BitT>& rhs) const { return Flags<BitT>(mask_ & rhs.mask_); }
  constexpr Flags<BitT> operator|(const Flags<BitT>& rhs) const { return Flags<BitT>(mask_ | rhs.mask_); }
  constexpr Flags<BitT> operator^(const Flags<BitT>& rhs) const { return Flags<BitT>(mask_ ^ rhs.mask_); }
  constexpr Flags<BitT> operator~() const { return Flags<BitT>(~mask_); }
  constexpr Flags<BitT>& operator=(const Flags<BitT>& rhs) = default;
  constexpr Flags<BitT>& operator&=(const Flags<BitT>& rhs) { return (*this = (*this & rhs)); }
  constexpr Flags<BitT>& operator|=(const Flags<BitT>& rhs) { return (*this = (*this | rhs)); }
  constexpr Flags<BitT>& operator^=(const Flags<BitT>& rhs) { return (*this = (*this ^ rhs)); }

  friend constexpr Flags<BitT> operator&(BitT lhs, Flags<BitT> rhs) { return rhs & lhs; }
  friend constexpr Flags<BitT> operator|(BitT lhs, Flags<BitT> rhs) { return rhs | lhs; }
  friend constexpr Flags<BitT> operator^(BitT lhs, Flags<BitT> rhs) { return rhs ^ lhs; }

  constexpr operator bool() const { return !!mask_; }
  constexpr operator MaskT() const { return mask_; }

  constexpr uint32_t bitsInCommon(const Flags<BitT>& other) const {
    uint32_t result {0};
    for (MaskT bit {0}; bit < sizeof(BitT)*8; ++bit) {
      if ((mask_ & MaskT(1 << bit)) && (other & BitT(1 << bit)))
        ++result;
    }
    return result;
  }

private:
  MaskT mask_ {0};
};

// Template specialization to determine whether a type should be considered a bitfield.
// To make a flag enum usable as a bitfield, you would use the following:
//   template <> struct isBitfield<MyEnumFlag> : public std::true_type {};
//   using MyEnumFlags = Flags<MyEnumFlag>;
template <typename BitT> struct isBitfield : public std::false_type {};

// Providing the bitwise and operator for custom bitfield enums.
template <typename BitT> requires (isBitfield<BitT>::value)
constexpr Flags<BitT> operator &(BitT lhs, BitT rhs) {
  return Flags<BitT>(static_cast<std::underlying_type_t<BitT>>(lhs) & static_cast<std::underlying_type_t<BitT>>(rhs));
}

// Providing the bitwise or operator for custom bitfield enums.
template <typename BitT> requires (isBitfield<BitT>::value)
constexpr Flags<BitT> operator |(BitT lhs, BitT rhs) {
  return Flags<BitT>(static_cast<std::underlying_type_t<BitT>>(lhs) | static_cast<std::underlying_type_t<BitT>>(rhs));
}

// Providing the bitwise xor operator for custom bitfield enums.
template <typename BitT> requires (isBitfield<BitT>::value)
constexpr Flags<BitT> operator ^(BitT lhs, BitT rhs) {
  return Flags<BitT>(static_cast<std::underlying_type_t<BitT>>(lhs) ^ static_cast<std::underlying_type_t<BitT>>(rhs));
}

// Providing the bitwise not operator for custom bitfield enums.
template <typename BitT> requires (isBitfield<BitT>::value)
constexpr Flags<BitT> operator ~(BitT lhs) {
  return Flags<BitT>(~static_cast<std::underlying_type_t<BitT>>(lhs));
}

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
};

// A predefined BlendSettings appropriate for common additive blending.
constexpr BlendSettings blendAdditive {
  .srcColorFactor = BlendFactor::SrcAlpha,
  .dstColorFactor = BlendFactor::One,
  .colorOp = BlendOp::Add,
  .srcAlphaFactor = BlendFactor::SrcAlpha,
  .dstAlphaFactor = BlendFactor::Zero
};

// A predefined BlendSettings appropriate for common alpha-interpolation blending.
constexpr BlendSettings blendAlpha {
  .srcColorFactor = BlendFactor::SrcAlpha,
  .dstColorFactor = BlendFactor::OneMinusSrcAlpha,
  .colorOp = BlendOp::Add,
  .srcAlphaFactor = BlendFactor::OneMinusSrcAlpha,
  .dstAlphaFactor = BlendFactor::Zero
};

// How should this buffer be used?
enum class BufferUsage : uint32_t {
  None              = 0,
  DeviceAddressable = 1 << 1, // The buffer's device address can be retrieved and used.
  HostMemory        = 1 << 2, // The buffer will exist on host memory (system ram) instead of on the GPU's VRAM.
  Index             = 1 << 3, // The buffer will contain indices.
  Storage           = 1 << 4, // The buffer will be used to arbitrary data.
  TransferSrc       = 1 << 5, // The buffer will be used as the source for transfer operations.
  TransferDst       = 1 << 6, // The buffer will be used as the destination for transfer operations.
  Uniform           = 1 << 7, // The buffer will be used as a uniform buffer object.
  Vertex            = 1 << 8, // The buffer will contain vertices (not neccessary if using buffer device address)
};
template <> struct isBitfield<BufferUsage> : public std::true_type {};
using BufferUsages = Flags<BufferUsage>;

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
  Trace,
  Debug,
  Info,
  Warning,
  Error
};
using DebugCallback = std::function<void(DebugSeverity, std::string_view)>;

// A {float, int} pair used for a clear value for a depth-stencil attachment.
struct DepthStencilClearVal { float depth {0.0f}; uint32_t stencil {0}; };

// A device-side pointer to somewhere in VRAM memory.  Used for bindless data.
using DeviceAddress = uint64_t;

// Features which don't need to be supported by a GPU to use HLGL, but may be requested and used by the user.
enum class Feature : uint32_t {
  None                = 0,
  BindlessTextures    = 1 << 0,
  BufferDeviceAddress = 1 << 1,
  DisplayHdr          = 1 << 2,
  DisplayVsync        = 1 << 3,
  Imgui               = 1 << 4,
  MeshShading         = 1 << 5,
  Raytracing          = 1 << 6,
  SamplerMinMax       = 1 << 7,
  ShaderObjects       = 1 << 8,
  Validation          = 1 << 8,
};
template <> struct isBitfield<Feature> : public std::true_type {};
using Features = Flags<Feature>;

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
  RGBA8i,
  BGR8i,
  BGRA8i,
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
template <> struct isBitfield<ShaderStage> : public std::true_type {};
using ShaderStages = Flags<ShaderStage>;

// The vendor which produced the GPU being used.
enum class Vendor : uint8_t {
  Other,
  AMD,
  ARM,
  ImgTec,
  INTEL,
  NVIDIA,
  Qualcomm
};

// A simple {major, minor, patch} tuple for passing around versions of things.
struct Version {
  uint32_t major {0}, minor {0}, patch {0};
};

struct Viewport {
  int32_t x {0}, y {0};
  uint32_t w {0}, h {0};
};

// WrapMode defines how texture addressing should handle values beyond the [0,1] range.
enum class WrapMode {
  DontCare,
  Repeat,
  MirrorRepeat,
  ClampToEdge,
  ClampToBorder,
  MirrorClampToEdge,
};

} // namespace wcgl

// The definition of WindowHandle is platform-dependent.
// Forward-declaring structs here lets us avoid including potentially problematic system headers (lookin' at you, windows.h).

#if defined WCGL_WINDOW_LIBRARY_GLFW
struct GLFWwindow;
namespace wcgl { using WindowHandle = GLFWwindow*; }
#elif defined WCGL_WINDOW_LIBRARY_SDL
struct SDL_Window;
namespace wcgl { using WindowHandle = SDL_Window*; }
#elif defined WCGL_WINDOW_LIBARY_NATIVE_WIN32
namespace wcgl { using WindowHandle = void*; }
#endif
