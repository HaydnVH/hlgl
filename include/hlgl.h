#ifndef HLGL_H
#define HLGL_H

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace hlgl {

///////////////////////////////////////////////////////////////////////////////
// Flags - Utility struct for managing bit flags.

template <typename BitType>
struct FlagsTraits {
  static constexpr bool isFlagBits = false;
};

template <typename BitType>
struct Flags {
  using MaskType = typename std::underlying_type<BitType>::type;

  // Constructors
  constexpr Flags() noexcept : mask_(0) {}
  constexpr Flags(BitType bit) noexcept : mask_(static_cast<MaskType>(bit)) {}
  constexpr Flags(const Flags<BitType>& rhs) noexcept = default;
  constexpr explicit Flags(MaskType flags) noexcept : mask_(flags) {}

  // Relational operators
  #if (true) // TODO: Add a check for whether this version of C++ supports the spaceship operator.
  auto operator <=>(const Flags<BitType>&) const = default;
  #else
  constexpr bool operator < (const Flags<BitType>& rhs) const noexcept { return mask_ <  rhs.mask_; }
  constexpr bool operator <=(const Flags<BitType>& rhs) const noexcept { return mask_ <= rhs.mask_; }
  constexpr bool operator > (const Flags<BitType>& rhs) const noexcept { return mask_ >  rhs.mask_; }
  constexpr bool operator >=(const Flags<BitType>& rhs) const noexcept { return mask_ >= rhs.mask_; }
  constexpr bool operator ==(const Flags<BitType>& rhs) const noexcept { return mask_ == rhs.mask_; }
  constexpr bool operator !=(const Flags<BitType>& rhs) const noexcept { return mask_ != rhs.mask_; }
  #endif

  // Logical operator
  constexpr bool operator!() const noexcept { return !mask_; }

  // Bitwise operators
  constexpr Flags<BitType> operator |(const Flags<BitType>& rhs) const noexcept { return Flags<BitType>(mask_ | rhs.mask_); }
  constexpr Flags<BitType> operator &(const Flags<BitType>& rhs) const noexcept { return Flags<BitType>(mask_ & rhs.mask_); }
  constexpr Flags<BitType> operator ^(const Flags<BitType>& rhs) const noexcept { return Flags<BitType>(mask_ ^ rhs.mask_); }
  constexpr Flags<BitType> operator ~() const noexcept { return Flags<BitType>(mask_ ^ ((1 << FlagsTraits<BitType>::numBits) - 1)); }

  // Assignment operators
  constexpr Flags<BitType>& operator  =(const Flags<BitType>& rhs) noexcept = default;
  constexpr Flags<BitType>& operator |=(const Flags<BitType>& rhs) noexcept { mask_ |= rhs.mask_; return *this; }
  constexpr Flags<BitType>& operator &=(const Flags<BitType>& rhs) noexcept { mask_ &= rhs.mask_; return *this; }
  constexpr Flags<BitType>& operator ^=(const Flags<BitType>& rhs) noexcept { mask_ ^= rhs.mask_; return *this; }

  // Cast operators
  constexpr operator bool() const noexcept { return (mask_ != 0); }
  constexpr operator MaskType() const noexcept { return mask_; }

  private:
  MaskType mask_;
};

// Bitwise operators
template <typename BitType>
constexpr Flags<BitType> operator |(BitType bit, const Flags<BitType>& rhs) noexcept { return rhs.operator|(bit); }
template <typename BitType>
constexpr Flags<BitType> operator &(BitType bit, const Flags<BitType>& rhs) noexcept { return rhs.operator&(bit); }
template <typename BitType>
constexpr Flags<BitType> operator ^(BitType bit, const Flags<BitType>& rhs) noexcept { return rhs.operator^(bit); }

// Bitwise operators on BitType
template <typename BitType, typename std::enable_if<FlagsTraits<BitType>::isFlags, bool>::type = true>
inline constexpr Flags<BitType> operator |(BitType lhs, BitType rhs) noexcept { return Flags<BitType>(lhs) | rhs; }
template <typename BitType, typename std::enable_if<FlagsTraits<BitType>::isFlags, bool>::type = true>
inline constexpr Flags<BitType> operator &(BitType lhs, BitType rhs) noexcept { return Flags<BitType>(lhs) & rhs; }
template <typename BitType, typename std::enable_if<FlagsTraits<BitType>::isFlags, bool>::type = true>
inline constexpr Flags<BitType> operator ^(BitType lhs, BitType rhs) noexcept { return Flags<BitType>(lhs) ^ rhs; }
template <typename BitType, typename std::enable_if<FlagsTraits<BitType>::isFlags, bool>::type = true>
inline constexpr Flags<BitType> operator ~(BitType bit) noexcept { return ~(Flags<BitType>(bit)); }



///////////////////////////////////////////////////////////////////////////////
// Type definitions

// The source and destination color and alpha blending factors.
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
  OneMinusSrc1Alpha };
constexpr inline const char* enumToStr(BlendFactor val) {
  switch (val) {
    case BlendFactor::Zero: return "Zero";
    case BlendFactor::One: return "One";
    case BlendFactor::SrcColor: return "SrcColor";
    case BlendFactor::OneMinusSrcColor: return "OneMinusSrcColor";
    case BlendFactor::DstColor: return "DstColor";
    case BlendFactor::OneMinusDstColor: return "OneMinusDstColor";
    case BlendFactor::SrcAlpha: return "SrcAlpha";
    case BlendFactor::OneMinusSrcAlpha: return "OneMinusSrcAlpha";
    case BlendFactor::DstAlpha: return "DstAlpha";
    case BlendFactor::OneMinusDstAlpha: return "OneMinusDstAlpha";
    case BlendFactor::ConstColor: return "ConstColor";
    case BlendFactor::OneMinusConstColor: return "OneMinusConstColor";
    case BlendFactor::ConstAlpha: return "ConstAlpha";
    case BlendFactor::OneMinusConstAlpha: return "OneMinusConstAlpha";
    case BlendFactor::SrcAlphaSaturate: return "SrcAlphaSaturate";
    case BlendFactor::Src1Color: return "Src1Color";
    case BlendFactor::OneMinusSrc1Color: return "OneMinusSrc1Color";
    case BlendFactor::Src1Alpha: return "Src1Alpha";
    case BlendFactor::OneMinusSrc1Alpha: return "OneMinusSrc1Alpha";
  }
}

// The blending operation between blend factors.
enum class BlendOp : uint8_t {
  Add,
  Subtract,
  SubtractReverse,
  Max,
  Min };
constexpr inline const char* enumToStr(BlendOp val) {
  switch (val) {
    case BlendOp::Add: return "Add";
    case BlendOp::Subtract: return "Subtract";
    case BlendOp::SubtractReverse: return "SubtractReverse";
    case BlendOp::Max: return "Max";
    case BlendOp::Min: return "Min";
  }
}

// A collection of BlendFactors and BlendOps which together define a blending state.
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

// Buffer represents a block of arbitrary GPU data.
class Buffer;
using BufferShared = std::shared_ptr<Buffer>;
using BufferUnique = std::shared_ptr<Buffer>;

// How should this buffer be used?
enum class BufferUsage: uint32_t {
  None              = 0,
  DescriptorHeap    = 1 << 1, // Storage for a descriptor heap.
  DeviceAddressable = 1 << 2, // The buffer's device address can be retrieved and used.
  HostVisible       = 1 << 3, // A host-visible buffer can have its contents be written to by memcpy.
  Index             = 1 << 4, // Index buffers store indices for indexed drawing.
  Storage           = 1 << 5, // Storage buffers are for arbitrary data storage.
  TransferSrc       = 1 << 6, // Valid source for transfer operations.
  TransferDst       = 1 << 7, // Valid destination for transfer operations.
  Uniform           = 1 << 8, // Uniform buffers are shared across draws and are typically updated every frame by the CPU.
  Updateable        = 1 << 9, // This buffer can be updated each frame.
  Vertex            = 1 << 10,// The buffer will contain vertices (not neccessary if using buffer device address).
  };
using BufferUsages = Flags<BufferUsage>;
template <> struct FlagsTraits<BufferUsage> { static constexpr bool isFlags {true}; static constexpr int32_t numBits {11}; };

// Describe the color framebuffers that a drawing pass will render to.
struct ColorAttachment {
  Image* image {nullptr};                         // The image which will serve as the render target. Required!
  std::optional<ColorRGBAf> clear {std::nullopt}; // The clear color for this attachment.  Optional, defaults to nullopt which disables clearing and loads the image's existing color.
};

// When a graphics pipeline is created, it needs to know about the color attachments it'll be rendering to.
struct ColorAttachmentInfo {
  ImageFormat format {ImageFormat::Undefined};          // The expected format for this color attachment.  Required!
  std::optional<BlendSettings> blending {std::nullopt}; // Blend settings for this color attachment.  Optional, defaults to nullopt which disables blending.
};

template <typename T, size_t N> requires(std::is_arithmetic_v<T> && N<=4)
class Color : public std::array<T,N> {
  public:
        T& r()       requires(N>=1) { return this->template at(0); }
  const T& r() const requires(N>=1) { return this->template at(0); }
        T& g()       requires(N>=2) { return this->template at(1); }
  const T& g() const requires(N>=2) { return this->template at(1); }
        T& b()       requires(N>=3) { return this->template at(2); }
  const T& b() const requires(N>=3) { return this->template at(2); }
        T& a()       requires(N>=4) { return this->template at(3); }
  const T& a() const requires(N>=4) { return this->template at(3); }
};

using ColorRGBb = Color<uint8_t, 3>;   // A 3-component byte color.
using ColorRGBAb = Color<uint8_t, 4>;  // A 4-component byte color.
using ColorRGBf = Color<float, 3>;     // A 3-component floating-point color.
using ColorRGBAf = Color<float, 4>;    // A 4-component floating-point color.
using ColorRGBi = Color<int32_t, 3>;   // A 3-component integer color.
using ColorRGBAi = Color<int32_t, 4>;  // A 4-component integer color.

// Comparison operator used for depth testing.
enum class CompareOp : uint8_t {
  Less,
  Greater,
  Equal,
  LessOrEqual,
  GreaterOrEqual,
  NotEqual,
  Always,
  Never };
constexpr inline const char* enumToStr(CompareOp val) {
  switch (val) {
    case CompareOp::Less: return "Less";
    case CompareOp::Greater: return "Greater";
    case CompareOp::Equal: return "Equal";
    case CompareOp::LessOrEqual: return "LessOrEqual";
    case CompareOp::GreaterOrEqual: return "GreaterOrEqual";
    case CompareOp::NotEqual: return "NotEqual";
    case CompareOp::Always: return "Always";
    case CompareOp::Never: return "Never";
  }
}

// A compute pipeline represents a compute shader and any associated state required to execute the compute shader.
class ComputePipeline;
using ComputePipelineShared = std::shared_ptr<ComputePipeline>;
using ComputePipelineUnique = std::unique_ptr<ComputePipeline>;

// The faces which should be culled when drawing.
enum class CullMode : uint8_t {
  None,
  Back,
  Front,
  FrontAndBack };
constexpr inline const char* enumToStr(CullMode val) {
  switch (val) {
    case CullMode::None: return "None";
    case CullMode::Back: return "Back";
    case CullMode::Front: return "Front";
    case CullMode::FrontAndBack: return "FrontAndBack";
  }
}

// The severity of a debug message being printed.
enum class DebugSeverity : uint8_t {
  Verbose,
  Info,
  Warning,
  Error,
  Fatal };
constexpr inline const char* enumToStr(DebugSeverity val) {
  switch (val) {
    case DebugSeverity::Verbose: return "Verbose";
    case DebugSeverity::Info: return "Info";
    case DebugSeverity::Warning: return "Warning";
    case DebugSeverity::Error: return "Error";
    case DebugSeverity::Fatal: return "Fatal";
  }
}

// Function pointer used to print debug messages.
using DebugCallbackFunc = void(*)(DebugSeverity, std::string_view);

// Describe the depth-stencil buffer that a drawing pass will render to.
struct DepthAttachment {
  Image* image {nullptr};                                   // The image which will serve as this pass's depth-stencil buffer.  Required!
  std::optional<DepthStencilClearVal> clear {std::nullopt}; // The clear depth/stencil value for this attachment.  Optional, defaults to nullopt which disables clearing and loads the image's existing depth/stencil value.
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

// A {float, int} pair used for a clear value for a depth-stencil attachment.
struct DepthStencilClearVal { float depth {0.0f}; uint32_t stencil {0}; };

// A device-side pointer to somewhere in VRAM memory.  Used for bindless data.
using DeviceAddress = uint64_t;

// This type is used for sizes and offsets in device memory.
using DeviceSize = uint64_t;

// Features which don't need to be supported by a GPU to use HLGL, but may be requested and used by the user.
enum class Feature : uint32_t {
  None                = 0,
  // Descriptor heaps are a new way of handling Vulkan descriptors to bring them more in line with how D3D12 manages descriptors.
  // While this will likely end up being the definitive way to handle descriptors in Vulkan, for now the driver support isn't broad enough to be reliable.
  DescriptorHeaps     = 1 << 1,
  // Task and Mesh shaders are a total replacement for traditional graphics pipeline shaders (except fragment shaders).
  // They are advanced and powerful but only supported on relatively new hardware.
  MeshShading         = 1 << 2,
  // Ray tracing is only supported by relatively new hardware, and can have significant performance drawbacks even when "properly" supported.
  RayTracing          = 1 << 3,
  // Validation involves requesting layers and extensions which can diminish runtime performance,
  // but are vital for properly debugging an application while it's in development.
  // Ray tracing is currently not implemented, so enabling this feature will do nothing.
  Validation          = 1 << 4,
  };
using Features = Flags<Feature>;
template <> struct FlagsTraits<Feature> { static constexpr bool isFlags {true}; static constexpr int32_t numBits {5}; };

// Texture filtering when sampled.
enum class FilterMode : uint8_t {
  DontCare,
  Nearest,
  Linear,
  Min,
  Max };
constexpr inline const char* enumToStr(FilterMode val) {
  switch (val) {
    case FilterMode::DontCare: return "DontCare";
    case FilterMode::Nearest: return "Nearest";
    case FilterMode::Linear: return "Linear";
    case FilterMode::Min: return "Min";
    case FilterMode::Max: return "Max";
  }
}

// Frame manages per-frame state and is needed for commands which need to be recorded to a command buffer.
class Frame;

// The front or back of a triangle is defined by its winding order.
enum class FrontFace : uint8_t {
  CounterClockwise,
  Clockwise };
constexpr inline const char* enumToStr(FrontFace val) {
  switch (val) {
    case FrontFace::CounterClockwise: return "CounterClockwise";
    case FrontFace::Clockwise: return "Clockwise";
  }
}

// The type of a GPU, in ascending order of desirability.
enum class GpuType : uint8_t {
  Other = 0,
  Cpu,
  Virtual,
  Integrated,
  Discrete };
constexpr inline const char* enumToStr(GpuType val) {
  switch (val) {
    case GpuType::Other: return "Other";
    case GpuType::Cpu: return "Cpu";
    case GpuType::Virtual: return "Virtual";
    case GpuType::Integrated: return "Integrated";
    case GpuType::Discrete: return "Discrete";
  }
}

// The vendor which produced the GPU.
enum class GpuVendor : uint8_t {
  Other,
  AMD,
  ARM,
  ImgTec,
  INTEL,
  NVIDIA,
  Qualcomm };
constexpr inline const char* enumToStr(GpuVendor val) {
  switch (val) {
    case GpuVendor::Other: return "Other";
    case GpuVendor::AMD: return "AMD";
    case GpuVendor::ARM: return "ARM";
    case GpuVendor::ImgTec: return "ImgTec";
    case GpuVendor::INTEL: return "INTEL";
    case GpuVendor::NVIDIA: return "NVIDIA";
    case GpuVendor::Qualcomm: return "Qualcomm";
  }
}

// Properties of a physical GPU.
struct GpuProperties {
  std::string name {};                                                  // The name of the GPU.
  uint32_t apiVerMajor {0}, apiVerMinor {0}, apiVerPatch {0};           // Which API version is provided.
  uint32_t driverVerMajor {0}, driverVerMinor {0}, driverVerPatch {0};  // The version of the graphics driver.
  uint64_t deviceMemory {0};                                            // How much device-local VRAM the GPU has access to.
  GpuType type {GpuType::Other};                                        // The type (CPU, Virtual, Itegrated, or Discrete) of the GPU.
  GpuVendor vendor {GpuVendor::Other};                                  // The vendor that built the GPU.
  Features supportedFeatures {Feature::None};                           // Which features the GPU supports.
  Features enabledFeatures {Feature::None};                             // Which features the GPU supports and are currently enabled.
};

// A graphics pipeline contains a collection of shaders and state required to execute drawing operations and render pixels.
class GraphicsPipeline;
using GraphicsPipelineShared = std::shared_ptr<GraphicsPipeline>;
using GraphicsPipelineUnique = std::unique_ptr<GraphicsPipeline>;

// Image represents a block of GPU data which can be rendered to, sampled as a texture, etc.
class Image;
using ImageShared = std::shared_ptr<Image>;
using ImageUnique = std::unique_ptr<Image>;

// The pixel/texel format for an image/texture.
// Enum values are the same as VkFormat for compatability reasons.
enum class ImageFormat : uint32_t {
  Undefined = 0,      // Undefined format, equivalent to VK_FORMAT_UNDEFINED
  RG4i = 1,           // 4 bits for red and green (8 bits total), equivalent to VK_FORMAT_R4G4_UNORM_PACK8.
  RGBA4i = 2,         // 4 bits for red, green, blue, and alpha (16 bits total), equivalent to VK_FORMAT_R4G4B4A4_UNORM_PACK16.
  R5G6B5i = 4,        // 5 bits for red, 6 for green, 5 for blue (16 bits total), equivalent to VK_FORMAT_R5G6B5_UNORM_PACK16.
  RGB5A1i = 6,        // 5 bits for red, green, and blue, and 1 bit for alpha (16 bits total), equivalent to VK_FORMAT_R5G5B5A1_UNORM_PACK16.
  R8i = 9,            // 8 bits for red (8 bits total), equivalent to VK_FORMAT_R8_UNORM.
  RG8i = 16,          // 8 bits for red and green (16 bits total), equivalent to VK_FORMAT_R8G8_UNORM.
  RGB8i = 23,         // 8 bits for red, green, and blue (24 bits total), equivalent to VK_FORMAT_R8G8B8_UNORM.
  RGB8i_srgb = 29,    // 8 bits for red, green, and blue with sRGB color space (24 bits total), equivalent to VK_FORMAT_R8G8B8_SRGB.
  BGR8i = 30,         // 8 bits for blue, green, and red (24 bits total), equivalent to VK_FORMAT_B8G8R8_UNORM.
  BGR8i_srgb = 36,    // 8 bits for blue, green, and red with sRGB color space (24 bits total), equivalent to VK_FORMAT_B8G8R8_SRGB.
  RGBA8i = 37,        // 8 bits for red, green, blue, and alpha (32 bits total), equivalent to VK_FORMAT_R8G8B8A8_UNORM.
  RGBA8i_srgb = 43,   // 8 bits for red, green, blue with sRGB color space plus 8 bits for linear alpha (32 bits total), equivalent to VK_FORMAT_R8G8B8A8_SRGB.
  BGRA8i = 44,        // 8 bits for blue, green, red, and alpha (32 bits total), equivalent to VK_FORMAT_B8G8R8A8_UNORM.
  BGRA8i_srgb = 50,   // 8 bits for blue, green, red with sRGB color space plus 8 bits for linear alpha (32 bits total), equivalent to VK_FORMAT_B8G8R8A8_SRGB.
  A2RGB10i = 59,      // 2 bits for alpha plus 10 bits (int) of red, green, and blue (32 bits total), equivalent to VK_FORMAT_A2R10G10B10_UNORM_PACK32.
  A2BGR10i = 64,      // 2 bits for alpha plus 10 bits (int) of blue, green, and red (32 bits total), equivalent to VK_FORMAT_A2B10G10R10_UNORM_PACK32.
  R16i = 70,          // 16 bits (int) for red (16 bits total), equivalent to VK_FORMAT_R16_UNORM.
  R16f = 76,          // 16 bits (float) for red (16 bits total), equivalent to VK_FORMAT_R16_SFLOAT.
  RG16i = 77,         // 16 bits (int) for red and green (32 bits total), equivalent to VK_FORMAT_R16G16_UNORM.
  RG16f = 83,         // 16 bits (float) for red and green (32 bits total), equivalent to VK_FORMAT_R16G16_SFLOAT.
  RGB16i = 84,        // 16 bits (int) for red, green, and blue (48 bits total), equivalent to VK_FORMAT_R16G16B16_UNORM.
  RGB16f = 90,        // 16 bits (float) for red, green, and blue (48 bits total), equivalent to VK_FORMAT_R16G16B16_SFLOAT.
  RGBA16i = 91,       // 16 bits (int) for red, green, blue, and alpha (64 bits total), equivalent to VK_FORMAT_R16G16B16A16_UNORM.
  RGBA16f = 97,       // 16 bits (float) for red, green, blue, and alpha (64 bits total), equivalent to VK_FORMAT_R16G16B16A16_SFLOAT.
  R32i = 98,          // 32 bits (int) for red (32 bits total), equivalent to VK_FORMAT_R32_UINT.
  R32f = 100,         // 32 bits (float) for red (32 bits total), equivalent to VK_FORMAT_R32_SFLOAT.
  RG32i = 101,        // 32 bits (int) for red and green (64 bits total), equivalent to VK_FORMAT_R32G32_UINT.
  RG32f = 103,        // 32 bits (float) for red and green (64 bits total), equivalent to VK_FORMAT_R32G32_SFLOAT.
  RGB32i = 104,       // 32 bits (int) for red, green, and blue (96 bits total), equivalent to VK_FORMAT_R32G32B32_UINT.
  RGB32f = 106,       // 32 bits (float) for red, green, and blue (96 bits total), equivalent to VK_FORMAT_R32G32B32_SFLOAT.
  RGBA32i = 107,      // 32 bits (int) for red, green, blue, and alpha (128 bits total), equivalent to VK_FORMAT_R32G32B32A32_UINT.
  RGBA32f = 109,      // 32 bits (float) for red, green, blue, and alpha (128 bits total), equivalent to VK_FORMAT_R32G32B32A32_SFLOAT.
  B10GR11f = 122,     // 10 bits (float) for blue, 11 for green and red (32 bits total), equivalent to VK_FORMAT_B10G11R11_UFLOAT_PACK32.
  D32f = 126,         // Depth-Stencil format, 32 bits (float) for depth and no stencil (32 bits total), equivalent to VK_FORMAT_D32_SFLOAT.
  D24S8 = 129,        // Depth-Stencil format, 24 bits (int) for depth and 8 bits for stencil (32 bits total), equivalent to VK_FORMAT_D24_UNORM_S8_UINT.
  D32fS8 = 130,       // Depth-Stencil format, 32 bits (float) for depth and 8 bits for stencil (64 bits total, probably), equivalent to VK_FORMAT_D32_SFLOAT_S8_UINT.
  COMPRESSED = 131,   // Not a real format.  Any formats greater than or equal to this value are compressed.
  BC1RGB = 131,       // Compressed format, BC1 (aka DXT1) has linear color and no alpha, equivalent to VK_FORMAT_BC1_RGB_UNORM_BLOCK.
  BC1RGB_srgb = 132,  // Compressed format, BC1 (aka DXT1) has sRGB color and no alpha, equivalent to VK_FORMAT_BC1_RGB_SRGB_BLOCK .
  BC1RGBA = 133,      // Compressed format, BC1 (aka DXT1) has linear color and 1-bit alpha, equivalent to VK_FORMAT_BC1_RGBA_UNORM_BLOCK.
  BC1RGBA_srgb = 134, // Compressed format, BC1 (aka DXT1) has sRGB color and 1-bit alpha, equivalent to VK_FORMAT_BC1_RGBA_SRGB_BLOCK.
  BC2 = 135,          // Compressed format, BC2 (aka DXT3) has linear color and explicit alpha, equivalent to VK_FORMAT_BC2_UNORM_BLOCK.
  BC2_srgb = 136,     // Compressed format, BC2 (aka DXT3) has sRGB color and explicit alpha, equivalent to VK_FORMAT_BC2_SRGB_BLOCK.
  BC3 = 137,          // Compressed format, BC3 (aka DXT5) has linear color and interpolated alpha, equivalent to VK_FORMAT_BC3_UNORM_BLOCK.
  BC3_srgb = 138,     // Compressed format, BC3 (aka DXT5) has sRGB color and interpolated alpha, equivalent to VK_FORMAT_BC3_SRGB_BLOCK.
  BC4u = 139,         // Compressed format, BC4 has one (unsigned) greyscale channel, equivalent to VK_FORMAT_BC4_UNORM_BLOCK.
  BC4s = 140,         // Compressed format, BC4 has one (signed) greyscale channel, equivalent to VK_FORMAT_BC4_SNORM_BLOCK.
  BC5u = 141,         // Compressed format, BC5 has two (unsigned) greyscale channels, equivalent to VK_FORMAT_BC5_UNORM_BLOCK.
  BC5s = 142,         // Compressed format, BC5 has two (signed) greyscale channels, equivalent to VK_FORMAT_BC5_SNORM_BLOCK.
  BC6u = 143,         // Compressed format, BC6 has HDR color and no alpha, equivalent to VK_FORMAT_BC6H_UFLOAT_BLOCK.
  BC6s = 144,         // Compressed format, BC6 has HDR color and no alpha, equivalent to VK_FORMAT_BC6H_SFLOAT_BLOCK.
  BC7 = 145,          // Compressed format, BC7 has linear color and interpolated alpha, equivalent to VK_FORMAT_BC7_UNORM_BLOCK.
  BC7_srgb = 146,     // Compressed format, BC7 has sRGB color and interpolated alpha, equivalent to VK_FORMAT_BC7_SRGB_BLOCK.
  ETC2RGB = 147,      // Compressed format, ETC2 has linear color and no alpha, equivalent to VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK.
  ETC2RGB_srgb = 148, // Compressed format, ETC2 has sRGB color and no alpha, equivalent to VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK.
  ETC2RGBA = 151,     // Compressed format, ETC2 has linear color alpha, equivalent to VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK.
  ETC2RGBA_srgb = 152,// Compressed format, ETC2 has sRGB color and linear alpha, equivalent to VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK.
  EACR11u = 153,      // Compressed format, EAC has one greyscale channel, equivalent to VK_FORMAT_EAC_R11_UNORM_BLOCK.
  EACR11s = 154,      // Compressed format, EAC has one greyscale channel, equivalent to VK_FORMAT_EAC_R11_SNORM_BLOCK.
  EACRG11u = 155,     // Compressed format, EAC has two greyscale channels, equivalent to VK_FORMAT_EAC_R11G11_UNORM_BLOCK.
  EACRG11s = 156,     // Compressed format, EAC has two greyscale channels, equivalent to VK_FORMAT_EAC_R11G11_SNORM_BLOCK.
  ASTC4x4 = 157,      // Compressed format, ASTC 4x4 has linear color and alpha, equivalent to VK_FORMAT_ASTC_4x4_UNORM_BLOCK.
  ASTC4x4_srgb = 158, // Compressed format, ASTC 4x4 has sRGB color and alpha, equivalent to VK_FORMAT_ASTC_4x4_SRGB_BLOCK.
  };
constexpr inline const char* enumToStr(ImageFormat val) {
  switch (val) {
    case ImageFormat::Undefined: return "Undefined";
    case ImageFormat::RG4i: return "RG4i";
    case ImageFormat::RGBA4i: return "RGBA4i";
    case ImageFormat::R5G6B5i: return "R5G6B5i";
    case ImageFormat::RGB5A1i: return "RGB5A1i";
    case ImageFormat::R8i: return "R8i";
    case ImageFormat::RG8i: return "RG8i";
    case ImageFormat::RGB8i: return "RGB8i";
    case ImageFormat::RGB8i_srgb: return "RGB8i_srgb";
    case ImageFormat::BGR8i: return "BGR8i";
    case ImageFormat::BGR8i_srgb: return "BGR8i_srgb";
    case ImageFormat::RGBA8i: return "RGBA8i";
    case ImageFormat::RGBA8i_srgb: return "RGBA8i_srgb";
    case ImageFormat::BGRA8i: return "BGRA8i";
    case ImageFormat::BGRA8i_srgb: return "BGRA8i_srgb";
    case ImageFormat::R16i: return "R16i";
    case ImageFormat::R16f: return "R16f";
    case ImageFormat::RG16i: return "RG16i";
    case ImageFormat::RG16f: return "RG16f";
    case ImageFormat::RGB16i: return "RGB16i";
    case ImageFormat::RGB16f: return "RGB16f";
    case ImageFormat::RGBA16i: return "RGBA16i";
    case ImageFormat::RGBA16f: return "RGBA16f";
    case ImageFormat::R32i: return "R32i";
    case ImageFormat::R32f: return "R32f";
    case ImageFormat::RG32i: return "RG32i";
    case ImageFormat::RG32f: return "RG32f";
    case ImageFormat::RGB32i: return "RGB32i";
    case ImageFormat::RGB32f: return "RGB32f";
    case ImageFormat::RGBA32i: return "RGBA32i";
    case ImageFormat::RGBA32f: return "RGBA32f";
    case ImageFormat::A2RGB10i: return "A2RGB10i";
    case ImageFormat::A2BGR10i: return "A2BGR10i";
    case ImageFormat::B10GR11f: return "B10GR11f";
    case ImageFormat::D32f: return "D32f";
    case ImageFormat::D24S8: return "D24S8";
    case ImageFormat::D32fS8: return "D32fS8";
    case ImageFormat::BC1RGB: return "BC1RGB";
    case ImageFormat::BC1RGB_srgb: return "BC1RGB_srgb";
    case ImageFormat::BC1RGBA: return "BC1RGBA";
    case ImageFormat::BC1RGBA_srgb: return "BC1RGBA_srgb";
    case ImageFormat::BC2: return "BC2";
    case ImageFormat::BC2_srgb: return "BC2_srgb";
    case ImageFormat::BC3: return "BC3";
    case ImageFormat::BC3_srgb: return "BC3_srgb";
    case ImageFormat::BC4u: return "BC4u";
    case ImageFormat::BC4s: return "BC4s";
    case ImageFormat::BC5u: return "BC5u";
    case ImageFormat::BC5s: return "BC5s";
    case ImageFormat::BC6u: return "BC6u";
    case ImageFormat::BC6s: return "BC6s";
    case ImageFormat::BC7: return "BC7";
    case ImageFormat::BC7_srgb: return "BC7_srgb";
    case ImageFormat::ETC2RGB: return "ETC2RGB";
    case ImageFormat::ETC2RGB_srgb: return "ETC2RGB_srgb";
    case ImageFormat::ETC2RGBA: return "ETC2RGBA";
    case ImageFormat::ETC2RGBA_srgb: return "ETC2RGBA_srgb";
    case ImageFormat::EACR11u: return "EACR11u";
    case ImageFormat::EACR11s: return "EACR11s";
    case ImageFormat::EACRG11u: return "EACRG11u";
    case ImageFormat::EACRG11s: return "EACRG11s";
    case ImageFormat::ASTC4x4: return "ASTC4x4";
    case ImageFormat::ASTC4x4_srgb: return "ASTC4x4_srgb";
  }
}

constexpr inline bool isFormatDepth(ImageFormat format) { return ((format == ImageFormat::D24S8) || (format == ImageFormat::D32f) || (format == ImageFormat::D32fS8)); }
constexpr inline bool isFormatCompressed(ImageFormat format) { return (static_cast<uint8_t>(format) > static_cast<uint8_t>(ImageFormat::COMPRESSED)); }

// How should this image be used?
enum class ImageUsage : uint16_t {
  None        = 0,
  Framebuffer = 1 << 0, // Framebuffer images can be used as attachments for drawing operations.
  Sampled     = 1 << 1, // Indicate that this image may be sampled as a texture in a shader.  This does not create a sampler object!
  ScreenSize  = 1 << 2, // Sets the size of the image to match the display, and resizes it whenever the display resizes.
  Storage     = 1 << 3, // A storage image can be used as arbitrary data storage by shaders.
  TransferSrc = 1 << 4, // Valid source for transfer operations.
  TransferDst = 1 << 5, // Valid destination for transfer operations.
  };
using ImageUsages = Flags<ImageUsage>;
template <> struct FlagsTraits<ImageUsage> { static constexpr bool isFlags {true}; static constexpr int32_t numBits {6}; };

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
  Patches };
constexpr inline const char* enumToStr(Primitive val) {
  switch (val) {
    case Primitive::Points: return "Points";
    case Primitive::Lines: return "Lines";
    case Primitive::LineStrip: return "LineStrip";
    case Primitive::Triangles: return "Triangles";
    case Primitive::TriangleStrip: return "TriangleStrip";
    case Primitive::TriangleFan: return "TriangleFan";
    case Primitive::LinesWithAdj: return "LinesWithAdj";
    case Primitive::LineStripWithAdj: return "LineStripWithAdj";
    case Primitive::TrianglesWithAdj: return "TrianglesWithAdj";
    case Primitive::TriangleStripWithAdj: return "TriangleStripWithAdj";
    case Primitive::Patches: return "Patches";
  }
}

// A raytracing pipeline contains a collection of shaders and state required to execute raytracing operations.
// TODO: Implement ray tracing.  This is just a placeholder for now.
// class RayTracingPipeline;
// using RayTracingPipelineShared = std::shared_ptr<RayTracingPipeline>;
// using RayTracingPipelineUnique = std::unique_ptr<RayTracingPipeline>;

// Sampler describes how the data stored in an Image should be accessed by shaders.
class Sampler;
using SamplerShared = std::shared_ptr<Sampler>;
using SamplerUnique = std::unique_ptr<Sampler>;

// The Shader class manages and compiles shaders, allowing them to be used for pipelines.  Once all pipelines using a given shader have been created, the Shader can be safely destroyed.
class Shader;
using ShaderShared = std::shared_ptr<Shader>;
using ShaderUnique = std::unique_ptr<Shader>;

// When a pipeline is created, ShaderInfo is used to provide a shader and its entrypoint.
struct ShaderInfo {
  Shader* shader {nullptr};
  const char* entry {"main"};
};

// The stage for which a shader will be used.
enum class ShaderStage : uint16_t {
  None            = 0,
  Vertex          = 1 << 0,
  Geometry        = 1 << 1,
  TessControl     = 1 << 2,
  TessEvaluation  = 1 << 3,
  Fragment        = 1 << 4,
  Compute         = 1 << 5,
  Task            = 1 << 6,
  Mesh            = 1 << 7,
  RayGen          = 1 << 8,
  Intersection    = 1 << 9,
  AnyHit          = 1 << 10,
  ClosestHit      = 1 << 11,
  Miss            = 1 << 12
  };
using ShaderStages = Flags<ShaderStage>;
template <> struct FlagsTraits<ShaderStage> { static constexpr bool isFlags {true}; static constexpr int32_t numBits {13}; };

struct Viewport {
  int32_t x {0}, y {0};
  uint32_t w {0}, h {0};
};

// Vsync mode, aka presentation mode.
enum class VsyncMode : uint8_t {
  Immediate,
  Fifo,
  FifoRelaxed,
  Mailbox };
constexpr inline const char* enumToStr(VsyncMode val) {
  switch (val) {
    case VsyncMode::Immediate: return "Immediate";
    case VsyncMode::Fifo: return "Fifo";
    case VsyncMode::FifoRelaxed: return "FifoRelaxed";
    case VsyncMode::Mailbox: return "Mailbox";
  }
}

// WrapMode defines how texture addressing should handle values beyond the [0,1] range.
enum class WrapMode : uint8_t {
  DontCare,
  Repeat,
  MirrorRepeat,
  ClampToEdge,
  ClampToBorder,
  MirrorClampToEdge, };
constexpr inline const char* enumToStr(WrapMode val) {
  switch (val) {
    case WrapMode::DontCare: return "DontCare";
    case WrapMode::Repeat: return "Repeat";
    case WrapMode::MirrorRepeat: return "MirrorRepeat";
    case WrapMode::ClampToEdge: return "ClampToEdge";
    case WrapMode::ClampToBorder: return "ClampToBorder";
    case WrapMode::MirrorClampToEdge: return "MirrorClampToEdge";
  }
}

} // namespace hlgl

// The definition of WindowHandle is platform-dependent.
// Forward-declaring structs here lets us avoid including potentially problematic system headers (lookin' at you, windows.h).

#if defined HLGL_WINDOW_LIBRARY_GLFW
struct GLFWwindow;
namespace hlgl { using WindowHandle = GLFWwindow*; }
#elif defined HLGL_WINDOW_LIBRARY_SDL
struct SDL_Window;
namespace hlgl { using WindowHandle = SDL_Window*; }
#else
namespace hlgl { using WindowHandle = void*; }
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////
// Functions

namespace hlgl {

struct          InitContextParams {
  WindowHandle window;                                                  // Handle/pointer to the window which the renderer should draw to.  Required!
  const char* appName {nullptr};                                        // Name of the application.  Optional.
  struct {uint32_t major {0}, minor {0}, patch {0};} appVer {};         // Version of the application.  Defaults to {0,0,0}.
  const char* engineName {nullptr};                                     // Name of the engine that the application is running on.  Optional.
  struct {uint32_t major {0}, minor {0}, patch {0};} engineVer {};      // Version of the engine that the application is running on.  Defaults to {0,0,0}.
  DebugCallbackFunc debugCallback {nullptr};                            // Callback function so HLGL can print messages to some output.  Optional, if not provided then HLGL wont print anything.
  const char* preferredGpu {nullptr};                                   // Name of the GPU which should be preferred regardless of capability.  Optional, when not provided (or not found) the best GPU will be chosen.
  Features preferredFeatures {Features::None};                          // The set of optional features which should be enabled if supported by the GPU.
  Features requiredFeatures {Features::None};                           // The set of features which must be enabled, causing initialization to fail in their absence.
  VsyncMode vsync {VsyncMode::Fifo};                                    // The Vsync mode which should be used initally.  This can be changed after context initialization.
  bool hdr {false};                                                     // Whether HDR should be enabled initially.  This can be changed after context intitialization.
  };
bool            initContext(InitContextParams params);                                    // Initialize the HLGL context.  Returns false if initialization fails, in which case the application should close.
void            shutdownContext();                                                        // Shuts down the HLGL context, cleaning up any remaining objects and GPU resources.

// Starts a new ImGui frame.
// This calls the appropriate backend's "*_NewFrame()" functions, as well as "ImGui::NewFrame()".
// After "hlgl::imguiNewFrame()", call your gui functions, followed by "ImGui::Render()".
// This should be done BEFORE the HLGL frame, which only draws the rendered ImGui state on top of the screen.
void            imguiNewFrame();

Frame*          beginFrame();                                                             // Begins a new frame and returns a pointer to the frame object used for the current frame, or null if this frame should be skipped.
bool            beginDrawing(Frame* frame,                                                // Binds the given attachments and begins a drawing pass.
                  std::initializer_list<ColorAttachment> colorAttachments,
                  std::optional<DepthAttachment> depthAttachment = std::nullopt);                                       
void            bindIndexBuffer(Frame* frame, Buffer* indexBuffer);                       // Binds the given index buffer to be used for subsequent drawIndexed calls.
void            bindPipeline(Frame* frame, ComputePipeline* pipeline);                    // Binds the given pipeline, allowing it to be used for dispatch calls.
void            bindPipeline(Frame* frame, GraphicsPipeline* pipeline);                   // Binds the given pipeline, allowing it to be used for drawing calls.
struct          BlitRegion {
  bool screenRegion {false}; // If true, the blit region will match the display size regardless of the image sizes.
  uint32_t mipLevel {0}, baseLayer {0}, layerCount {1};
  uint32_t x{0}, y{0}, z{0};
  uint32_t w{UINT32_MAX}, h{UINT32_MAX}, d{UINT32_MAX};
  };
void            blitImage(Frame* frame,                                                   // Blits (copies) the contents of one image (src) to another image (dst).
                  Image* dst, Image* src,
                  BlitRegion region);
void            dispatch(Frame* frame,                                                    // Executes the currently bound compute pipeline using the given group counts.
                  uint32_t groupCountX,
                  uint32_t groupCountY,
                  uint32_t groupCountZ);
void            draw(Frame* frame,                                                        // Draws 'vertexCount' vertices according to the currently bound graphics pipeline.
                  uint32_t vertexCount,
                  uint32_t instanceCount = 1,
                  uint32_t firstVertex = 0,
                  uint32_t firstInstance = 0);
void            drawIndexed(Frame* frame,                                                 // Draws 'indexCount' vertices according to the currently bound graphics pipeline and index buffer.
                  uint32_t indexCount,
                  uint64_t indexBufferOffset = 0,
                  uint32_t instanceCount = 1,
                  uint32_t firstIndex = 0,
                  uint32_t vertexOffset = 0,
                  uint32_t firstInstance = 0);
void            endDrawing(Frame* frame);                                                 // Ends the current drawing pass.
void            endFrame(Frame* frame);                                                   // Ends the frame, executing command buffers and displaying the swapchain image to the screen.
Image*          getFrameSwapchainImage(Frame* frame);                                     // Gets the current swapchain image which this frame will draw to.
void            pushConstants(Frame* frame, const void* data, size_t size);               // Pushes the provided data to the currently bound pipeline as a push constant block.
void            updateBufferData(Frame* frame,                                            // Updates the contents of the given buffer at the given offset using the given data and size.  Buffer must be updateable.
                  Buffer* buffer,                            
                  void* data,
                  size_t size,
                  DeviceSize offset);

struct          CreateBufferParams {
  BufferUsages  usage {BufferUsage::None};  // Usage flags
  struct Data { const void* ptr {nullptr}; size_t size {0}; };
  std::initializer_list<Data> data;         // Blocks of data to be copied into the new buffer and their sizes.
  DeviceSize    size {0};                   // Size of the buffer.  If 'data' is non-empty, this is added to the total size.
  uint32_t      indexSize {4};              // The number of bytes in each element of the index buffer.
  std::string   debugName {};               // Name used for debug messages.
  };
Buffer*         createBuffer(CreateBufferParams params, void* placement = nullptr);       // Create a buffer using the provided parameters, returning a raw pointer or null on failure.  If 'placement' is non-null, the Buffer will be created there.
BufferShared    createBufferShared(CreateBufferParams params);                            // Create a buffer using the provided parameters, returning a shared pointer or null on failure.
BufferUnique    createBufferUnique(CreateBufferParams params);                            // Create a buffer using the provided parameters, returning a unique pointer or null on failure.
void            destroyBuffer(Buffer* buffer);                                            // Destroy a buffer which was created using "createBuffer()".
size_t          sizeOfBuffer();                                                           // Returns the size, in bytes, of a Buffer object.

struct                  CreateComputePipelineParams {
  ShaderInfo compShader;  // Compute shader
  const char* debugName {nullptr};
  };
ComputePipeline*        createComputePipeline(CreateComputePipelineParams params);        // Create a compute pipeline using the provided parameters, returning a raw pointer or null on failure.
ComputePipelineShared   createComputePipelineShared(CreateComputePipelineParams params);  // Create a compute pipeline using the provided parameters, returning a shared pointer or null on failure.
ComputePipelineUnique   createComputePipelineUnique(CreateComputePipelineParams params);  // Create a compute pipeline using the provided parameters, returning a unique pointer or null on failure.
void                    destroyComputePipeline(ComputePipeline* pipeline);                // Destroy a compute pipeline which was created using "createComputePipeline()".

struct                  CreateGraphicsPipelineParams {
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
  };
GraphicsPipeline*       createGraphicsPipeline(CreateGraphicsPipelineParams params);      // Create a graphics pipeline using the provided parameters, returning a raw pointer or null on failure.
GraphicsPipelineShared  createGraphicsPipelineShared(CreateGraphicsPipelineParams params);// Create a graphics pipeline using the provided parameters, returning a shared pointer or null on failure.
GraphicsPipelineUnique  createGraphicsPipelineUnique(CreateGraphicsPipelineParams params);// Create a graphics pipeline using the provided parameters, returning a unique pointer or null on failure.
void                    destroyComputePipeline(GraphicsPipeline* pipeline);               // Destroy a graphics pipeline which was created using "createGraphicsPipeline()".

struct          CreateImageParams {
  ImageUsages usage {ImageUsage::None};         // Usage flags
  uint32_t    width {1};                        // Width of the texture, in pixels.  Cannot be 0.
  uint32_t    height {1};                       // Height of the texture, in pixels.  If 0 or 1, the texture will be 1D.
  uint32_t    depth {1};                        // Depth of the texture, in pixels.  If 0 or 1, the texture will be 1D or 2D.
  uint32_t    mipCount {1};                     // Number of mipmap levels in the texture.  Cannot be 0.  Defaults to 1 for no mipmapping.
  uint32_t    mipBase {0};                      // Base mipmap level.  Defaults to 0.
  uint32_t    layerCount {1};                   // Number of layers in the texture.  Defaults to 1 for a non-layered texture.
  uint32_t    layerBase {0};                    // Base layer index.  Defaults to 0.
  ImageFormat format {ImageFormat::Undefined};  // Pixel format.  Required!
  void*       dataPtr {nullptr};                // Pointer to image data to be copied into the image.
  uint64_t    dataSize {0};                     // Size of the image data to be copied into the image.
  uint64_t*   mipOffsets {nullptr};             // An array of 'mipCount' offsets into the data pointed to by 'dataPtr' indicating each mip level's region.
  std::string debugName {};
  };
Image*          createImage(CreateImageParams params, void* placement = nullptr);         // Create an image using the provided parameters, returning a raw pointer or null on failure.  If 'placement' is non-null, the Image will be created there.
ImageShared     createImageShared(CreateImageParams params);                              // Create an image using the provided parameters, returning a shared pointer or null on failure.
ImageUnique     createImageUnique(CreateImageParams params);                              // Create an image using the provided parameters, returning a unique pointer or null on failure.
void            destroyImage(Image* image);                                               // Destroy an image which was created using "createImage()".
size_t          sizeOfImage();                                                            // Returns the size, in bytes, of an Image object.

struct          CreateSamplerParams {
  ImageFormat format {ImageFormat::Undefined};    // Pixel format.  Required!
  WrapMode    wrapping {WrapMode::ClampToEdge};   // Wrapping mode to use along any axis which is set to 'Wrapping::DontCare'.  Defaults to ClampToEdge.
  WrapMode    wrapU {WrapMode::DontCare};         // Wrapping mode to use along the X (U) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
  WrapMode    wrapV {WrapMode::DontCare};         // Wrapping mode to use along the Y (V) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
  WrapMode    wrapW {WrapMode::DontCare};         // Wrapping mode to use along the Z (W) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
  FilterMode  filtering {FilterMode::Nearest};    // Filter operation to use for minifaction and magnification.  Defaults to Nearest.
  FilterMode  filterMin {FilterMode::DontCare};   // Filter operation to use for magnification.  Defaults to DontCare.
  FilterMode  filterMag {FilterMode::DontCare};   // Filter operation to use for magnification.  Defaults to DontCare.
  FilterMode  filterMips {FilterMode::DontCare};  // Filter operation to use between mipmaps.  Defaults to Nearest.
  float       maxAnisotropy {8.0f};
  float       maxLod {1.0f};                      // The maximum level of detail, which should be equal to the number of mip levels in the image (in most cases).
  ColorRGBAi  borderColor {255,255,255,255};      // Border color to use if wrap mode is ClampToBorder.
  };
Sampler*        createSampler(CreateSamplerParams params, void* placement = nullptr);     // Create a sampler using the provided parameters, returning a raw pointer or null on failure.  If 'placement' is non-null, the Sampler will be created there.
SamplerShared   createSamplerShared(CreateSamplerParams params);                          // Create a sampler using the provided parameters, returning a shared pointer or null on failure.
SamplerUnique   createSamplerUnique(CreateSamplerParams params);                          // Create a sampler using the provided parameters, returning a unique pointer or null on failure.
void            destroySampler(Sampler* sampler);                                         // Destroy a sampler which was created using "createSampler()".
size_t          sizeOfSampler();                                                          // Returns the size, in bytes, of a Sampler object.

struct          CreateShaderParams {
  const char* src {nullptr};      // The source code for this shader.  This code will be compiled to Spir-V and then used to create a shader module.
  const void* spvData {nullptr};  // Shaders can also be provided as precompiled Spir-V bytecode.  This is faster than compiling at runtime.
  size_t spvSize {0};             // The size, in bytes, of the provided Spir-V bytecode.
  const char* debugName {nullptr};
  };
Shader*         createShader(CreateShaderParams params);
ShaderShared    createShaderShared(CreateShaderParams params);
ShaderUnique    createShaderUnique(CreateShaderParams params);

DeviceSize      getBufferSize(Buffer* buffer);                                            // Gets the size, in bytes, of the given Buffer.
DeviceAddress   getBufferDeviceAddress(Buffer* buffer);                                   // Gets the device address of the given Buffer.  It must have been created with BufferUsage::DeviceAddressable.
float           getDisplayAspectRatio();                                                  // Gets the aspect ratio of the current display (width / height).
ImageFormat     getDisplayFormat();                                                       // Gets the image format of the display's surface.
void            getDisplaySize(uint32_t& w, uint32_t& h);                                 // Gets the of the display.  Width is stored in 'w' and height is stored in 'h'.
GpuProperties&  getGpuProperties();                                                       // Gets the properties of the GPU being used by HLGL.
void            getImageDimensions(Image* image, uint32_t& w, uint32_t& h, uint32_t& d);  // Gets the dimensions of the given image.  Width is stored in 'w', height in 'h', and depth in 'd'.
ImageFormat     getImageFormat(Image* image);                                             // Gets the format of the given image.
VsyncMode       getVsync();                                                               // Gets the current vsync mode.
bool            isDepthFormatSupported(ImageFormat format);                               // Returns true if the provided format is supported as a depth-stencil format by the GPU being used by HLGL.
bool            isGraphicsPipelineOpaque(GraphicsPipeline* pipeline);                     // Returns true if the pipeline is opaque (no blending or alpha testing enabled).
bool            isHdrEnabled();                                                           // Returns true if HDR rendering is currently enabled.

bool            recreateImage(Image* image);                                              // Recreate the image using the CreateImageParameters used to create it.  Returns true on success.  On failure, returns false and the image is unchanged.
bool            resizeImage(Image* image, uint32_t w, uint32_t h, uint32_t d);            // Recreate the image using new dimensions.  Returns true on success.  On failure, returns false and the image is unchanged.
void            setDisplaySize(uint32_t w, uint32_t h);                                   // After the display resizes, use this to provide a hint for what size the new swapchain should be.
void            setHdr(bool mode);                                                        // Sets whether to request an HDR surface.  If HDR support isn't available, it will be disabled.
void            setVsync(VsyncMode mode);                                                 // Sets the requested vsync mode.  If the requested mode isn't available, the swapchain may default to "Fifo".

} // namespace hlgl
#endif // HLGL_H
