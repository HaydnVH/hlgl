#ifndef HLGL_BASE_H
#define HLGL_BASE_H

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace hlgl {

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Flags - Utility struct for managing bit flags.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
  #if (__cplusplus >= 202002L) // If C++ std version is 20 or greater, use the spaceship operator.
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declaring core classes.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Buffer represents a block of arbitrary GPU data.
class Buffer;

// Texture represents image data on the GPU plus its view and sampler.
class Texture;

// A pipeline represents one or more shaders and any associated state required to execute the shaders.
class Pipeline;

// The Shader class manages and compiles shaders, allowing them to be used for pipelines.  Once all pipelines using a given shader have been created, the Shader can be safely destroyed.
class Shader;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Struct and enum type definitions.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The pixel/texel format for an image/texture.
// Enum values are the same as VkFormat for compatability reasons.
enum class ImageFormat {
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
constexpr inline bool isFormatCompressed(ImageFormat format) { return (static_cast<uint32_t>(format) >= static_cast<uint32_t>(ImageFormat::COMPRESSED)); }

// The source and destination color and alpha blending factors.
enum class BlendFactor {
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
enum class BlendOp {
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

using ColorRGBb = std::array<uint8_t, 3>;   // A 3-component byte color.
using ColorRGBAb = std::array<uint8_t, 4>;  // A 4-component byte color.
using ColorRGBf = std::array<float, 3>;     // A 3-component floating-point color.
using ColorRGBAf = std::array<float, 4>;    // A 4-component floating-point color.
using ColorRGBi = std::array<int32_t, 3>;   // A 3-component integer color.
using ColorRGBAi = std::array<int32_t, 4>;  // A 4-component integer color.

// Describe the color framebuffers that a drawing pass will render to.
struct ColorAttachment {
  Texture* texture {nullptr};                       // The image which will serve as the render target. Required!
  std::optional<ColorRGBAf> clear {std::nullopt}; // The clear color for this attachment.  Optional, defaults to nullopt which disables clearing and loads the image's existing color.
};

// Comparison operator used for depth testing.
enum class CompareOp {
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

// The faces which should be culled when drawing.
enum class CullMode {
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
enum class DebugSeverity {
  ObjectCreation,
  Verbose,
  Info,
  Warning,
  Error,
  Fatal };
constexpr inline const char* enumToStr(DebugSeverity val) {
  switch (val) {
    case DebugSeverity::ObjectCreation: return "ObjectCreation";
    case DebugSeverity::Verbose: return "Verbose";
    case DebugSeverity::Info: return "Info";
    case DebugSeverity::Warning: return "Warning";
    case DebugSeverity::Error: return "Error";
    case DebugSeverity::Fatal: return "Fatal";
  }
}

// Function pointer used to print debug messages.
using DebugCallbackFunc = void(*)(DebugSeverity, std::string_view);

// A {float, int} pair used for a clear value for a depth-stencil attachment.
struct DepthStencilClearVal { float depth {0.0f}; uint32_t stencil {0}; };

// Describe the depth-stencil buffer that a drawing pass will render to.
struct DepthAttachment {
  Texture* texture {nullptr};                                 // The image which will serve as this pass's depth-stencil buffer.  Required!
  std::optional<DepthStencilClearVal> clear {std::nullopt}; // The clear depth/stencil value for this attachment.  Optional, defaults to nullopt which disables clearing and loads the image's existing depth/stencil value.
};

// A device-side pointer to somewhere in VRAM memory.  Used for bindless data.
using DeviceAddress = uint64_t;

// This type is used for sizes and offsets in device memory.
using DeviceSize = uint64_t;

// A drawBuffer used for (indexed) indirect draw calls should be an array of this structure.
// You may use a larger struct with additional per-object data, and adjust "stride" accordingly, but this struct should come before that data.
struct DrawIndexedIndirectEntry {
  uint32_t indexCount;
  uint32_t instanceCount;
  uint32_t firstIndex;
  int32_t vertexOffset;
  uint32_t firstInstance;
};

// A drawBuffer used for (non-indexed) indirect draw calls should be an array of this structure.
// You may use a larger struct with additional per-object data, and adjust "stride" accordingly, but this struct should come before that data.
struct DrawIndirectEntry {
  uint32_t vertexCount;
  uint32_t instanceCount;
  uint32_t firstVertex;
  uint32_t firstInstance;
};

// Features which don't need to be supported by a GPU to use HLGL, but may be requested and used by the user.
enum class Feature {
  None                = 0,
  // Task and Mesh shaders are a total replacement for traditional graphics pipeline shaders (except fragment shaders).
  // They are advanced and powerful but only supported on relatively new hardware.
  MeshShading         = 1 << 1,
  // Ray tracing is only supported by relatively new hardware, and can have significant performance drawbacks even when "properly" supported.
  RayTracing          = 1 << 2,
  // Validation involves requesting layers and extensions which can diminish runtime performance,
  // but are vital for properly debugging an application while it's in development.
  // Ray tracing is currently not implemented, so enabling this feature will do nothing.
  Validation          = 1 << 3,
  };
using Features = Flags<Feature>;
template <> struct FlagsTraits<Feature> { static constexpr bool isFlags {true}; static constexpr int32_t numBits {4}; };

// Texture filtering when sampled.
enum class FilterMode {
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
enum class FrontFace {
  CounterClockwise,
  Clockwise };
constexpr inline const char* enumToStr(FrontFace val) {
  switch (val) {
    case FrontFace::CounterClockwise: return "CounterClockwise";
    case FrontFace::Clockwise: return "Clockwise";
  }
}

// The type of a GPU, in ascending order of desirability.
enum class GpuType {
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
enum class GpuVendor {
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

enum class ImageLayout {
  Undefined = 0,
  General = 1,
  ColorAttachment = 2,
  DepthStencilAttachment = 3,
  DepthStencilReadOnly = 4,
  ShaderReadOnly = 5,
  TransferSrc = 6,
  TransferDst = 7,
  Preinitialized = 8
  };
constexpr inline const char* enumToStr(ImageLayout val) {
  switch (val) {
    case ImageLayout::Undefined: return "Undefined";
    case ImageLayout::General: return "General";
    case ImageLayout::ColorAttachment: return "ColorAttachment";
    case ImageLayout::DepthStencilAttachment: return "DepthStencilAttachment";
    case ImageLayout::DepthStencilReadOnly: return "DepthStencilReadOnly";
    case ImageLayout::ShaderReadOnly: return "ShaderReadOnly";
    case ImageLayout::TransferSrc: return "TransferSrc";
    case ImageLayout::TransferDst: return "TransferDst";
    case ImageLayout::Preinitialized: return "Preinitialized";
  }
}

// The type of a pipeline.
enum class PipelineType {
  Compute,
  Graphics };
constexpr inline const char* enumToStr(PipelineType val) {
  switch (val) {
    case PipelineType::Compute: return "Compute";
    case PipelineType::Graphics: return "Graphics";
  }
}

// The type of geometry primitive to be drawn.
enum class Primitive {
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

enum class Result {
  Success = 0,
  SkipFrame,
  Shutdown };
constexpr inline const char* enumToStr(Result val) {
  switch (val) {
    case Result::Success: return "Success";
    case Result::SkipFrame: return "SkipFrame";
    case Result::Shutdown: return "Shutdown";
  }
}

// The stage for which a shader will be used.
enum class ShaderStage {
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

// When a pipeline is created, ShaderInfo is used to provide a shader and its entrypoint.
struct ShaderInfo {
  Shader* shader {nullptr};
  const char* entry {"main"};
  ShaderStages stage {ShaderStage::None};
};

struct Viewport {
  int32_t x {0}, y {0};
  uint32_t w {0}, h {0};
};

// Vsync mode, aka presentation mode.
enum class VsyncMode {
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
enum class WrapMode {
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
#elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
struct HWND__;
namespace hlgl { using WindowHandle = HWND__*; }
#else
namespace hlgl { using WindowHandle = void*; }
#endif

#endif // HLGL_BASE_H