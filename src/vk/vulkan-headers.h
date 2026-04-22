#ifndef HLGL_VK_VULKAN_HEADERS_H
#define HLGL_VK_VULKAN_HEADERS_H

#define VK_NO_PROTOTYPES
#include <volk.h>
#include <vk_mem_alloc.h>

#include <vulkan/vk_enum_string_helper.h>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#if defined HLGL_WINDOW_LIBRARY_GLFW
#include <backends/imgui_impl_glfw.h>
#elif defined HLGL_WINDOW_LIBRARY_SDL2
#include <backends/imgui_impl_sdl2.h>
#elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
#include <backends/imgui_impl_win32.h>
#endif

namespace hlgl {

void debugPrint(DebugSeverity severity, const char* fmt, ...);
  
inline bool checkVkResult(VkResult result, const char* expr, hlgl::DebugSeverity failSeverity, bool swapchain = false) {
  if (result == VK_SUCCESS || swapchain && (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR))
    return true;
  else {
    hlgl::debugPrint(failSeverity, "'%s' failed; returned '%s'.", expr, string_VkResult(result));
    return false;
  }
}

constexpr inline VkImageAspectFlags      translateAspect(hlgl::ImageFormat format) {
  switch (format) {
  case hlgl::ImageFormat::D32f:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  case hlgl::ImageFormat::D24S8:
  case hlgl::ImageFormat::D32fS8:
    return (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
  default:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  }
  }
constexpr inline VkImageAspectFlags      translateAspect(VkFormat format) {
  switch (format) {
  case VK_FORMAT_D32_SFLOAT:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
  default:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  }
  }
constexpr inline VkIndexType             translateIndexType(uint32_t size) {
  switch (size) {
  case 1: return VK_INDEX_TYPE_UINT8_EXT;
  case 2: return VK_INDEX_TYPE_UINT16;
  case 4: return VK_INDEX_TYPE_UINT32;
  default: return VK_INDEX_TYPE_NONE_KHR;
  }
  }
constexpr inline VkSampleCountFlagBits   translateMsaa(uint32_t samples) {
  switch (samples) {
  case 1: return VK_SAMPLE_COUNT_1_BIT;
  case 2: return VK_SAMPLE_COUNT_2_BIT;
  case 4: return VK_SAMPLE_COUNT_4_BIT;
  case 8: return VK_SAMPLE_COUNT_8_BIT;
  case 16: return VK_SAMPLE_COUNT_16_BIT;
  case 32: return VK_SAMPLE_COUNT_32_BIT;
  case 64: return VK_SAMPLE_COUNT_64_BIT;
  default: return VK_SAMPLE_COUNT_1_BIT;
  }
  }
constexpr inline VkSamplerMipmapMode     translateMipMode(hlgl::FilterMode mode) {
  switch (mode) {
  case hlgl::FilterMode::Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  case hlgl::FilterMode::Linear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  case hlgl::FilterMode::Min: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  case hlgl::FilterMode::Max: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  default: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }
  }
constexpr inline VkSamplerReductionMode  translateReduction(hlgl::FilterMode mode) {
  switch (mode) {
  case hlgl::FilterMode::Min: return VK_SAMPLER_REDUCTION_MODE_MIN;
  case hlgl::FilterMode::Max: return VK_SAMPLER_REDUCTION_MODE_MAX;
  default: return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
  }
}

constexpr inline VkBlendFactor           translate(hlgl::BlendFactor factor) {
  switch (factor) {
  case hlgl::BlendFactor::Zero:               return VK_BLEND_FACTOR_ZERO;
  case hlgl::BlendFactor::One:                return VK_BLEND_FACTOR_ONE;
  case hlgl::BlendFactor::SrcColor:           return VK_BLEND_FACTOR_SRC_COLOR;
  case hlgl::BlendFactor::OneMinusSrcColor:   return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  case hlgl::BlendFactor::DstColor:           return VK_BLEND_FACTOR_DST_COLOR;
  case hlgl::BlendFactor::OneMinusDstColor:   return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  case hlgl::BlendFactor::SrcAlpha:           return VK_BLEND_FACTOR_SRC_ALPHA;
  case hlgl::BlendFactor::OneMinusSrcAlpha:   return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  case hlgl::BlendFactor::DstAlpha:           return VK_BLEND_FACTOR_DST_ALPHA;
  case hlgl::BlendFactor::OneMinusDstAlpha:   return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  case hlgl::BlendFactor::ConstColor:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
  case hlgl::BlendFactor::OneMinusConstColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
  case hlgl::BlendFactor::ConstAlpha:         return VK_BLEND_FACTOR_CONSTANT_ALPHA;
  case hlgl::BlendFactor::OneMinusConstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
  case hlgl::BlendFactor::SrcAlphaSaturate:   return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
  case hlgl::BlendFactor::Src1Color:          return VK_BLEND_FACTOR_SRC1_COLOR;
  case hlgl::BlendFactor::OneMinusSrc1Color:  return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
  case hlgl::BlendFactor::Src1Alpha:          return VK_BLEND_FACTOR_SRC1_ALPHA;
  case hlgl::BlendFactor::OneMinusSrc1Alpha:  return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  default: return VK_BLEND_FACTOR_ZERO;
  }
  }
constexpr inline VkBlendOp               translate(hlgl::BlendOp op) {
  switch (op) {
  case hlgl::BlendOp::Add:              return VK_BLEND_OP_ADD;
  case hlgl::BlendOp::Subtract:         return VK_BLEND_OP_SUBTRACT;
  case hlgl::BlendOp::SubtractReverse:  return VK_BLEND_OP_REVERSE_SUBTRACT;
  case hlgl::BlendOp::Max:              return VK_BLEND_OP_MAX;
  case hlgl::BlendOp::Min:              return VK_BLEND_OP_MIN;
  default: return VK_BLEND_OP_ADD;
  }
  }
constexpr inline VkCompareOp             translate(hlgl::CompareOp op) {
  switch (op) {
  case hlgl::CompareOp::Less: return VK_COMPARE_OP_LESS;
  case hlgl::CompareOp::Greater: return VK_COMPARE_OP_GREATER;
  case hlgl::CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
  case hlgl::CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
  case hlgl::CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
  case hlgl::CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
  case hlgl::CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
  case hlgl::CompareOp::Never: return VK_COMPARE_OP_NEVER;
  default: return VK_COMPARE_OP_NEVER;
  }
  }
constexpr inline VkCullModeFlags         translate(hlgl::CullMode mode) {
  switch (mode) {
  case hlgl::CullMode::None: return VK_CULL_MODE_NONE;
  case hlgl::CullMode::Back: return VK_CULL_MODE_BACK_BIT;
  case hlgl::CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
  case hlgl::CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
  default: return VK_CULL_MODE_NONE;
  }
  }
constexpr inline VkFilter                translate(hlgl::FilterMode mode) {
  switch (mode) {
  case hlgl::FilterMode::Nearest: return VK_FILTER_NEAREST;
  case hlgl::FilterMode::Linear: return VK_FILTER_LINEAR;
  case hlgl::FilterMode::Min: return VK_FILTER_LINEAR;
  case hlgl::FilterMode::Max: return VK_FILTER_LINEAR;
  default: return VK_FILTER_NEAREST;
  }
  }
constexpr inline VkFormat                translate(hlgl::ImageFormat format) {
  return static_cast<VkFormat>(format);
  }
constexpr inline hlgl::ImageFormat       translate(VkFormat format) {
  return static_cast<hlgl::ImageFormat>(format);
  }
constexpr inline VkFrontFace             translate(hlgl::FrontFace front) {
  switch (front) {
  case hlgl::FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  case hlgl::FrontFace::Clockwise: return VK_FRONT_FACE_CLOCKWISE;
  default: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  }
  }
constexpr inline VkImageLayout           translate(hlgl::ImageLayout layout) {
  return static_cast<VkImageLayout>(layout);
  }
constexpr inline VkPrimitiveTopology     translate(hlgl::Primitive mode) {
  switch (mode) {
  case hlgl::Primitive::Points:  return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  case hlgl::Primitive::Lines:  return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  case hlgl::Primitive::LineStrip:  return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  case hlgl::Primitive::Triangles:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case hlgl::Primitive::TriangleStrip:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  case hlgl::Primitive::TriangleFan:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
  case hlgl::Primitive::LinesWithAdj:  return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
  case hlgl::Primitive::LineStripWithAdj:  return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
  case hlgl::Primitive::TrianglesWithAdj:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
  case hlgl::Primitive::TriangleStripWithAdj:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
  case hlgl::Primitive::Patches:  return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
  default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }
  }
constexpr inline VkShaderStageFlagBits   translate(hlgl::ShaderStages stages) {
  uint32_t result {0};
  if (stages & ShaderStage::Vertex)         { result |= VK_SHADER_STAGE_VERTEX_BIT; }
  if (stages & ShaderStage::Geometry)       { result |= VK_SHADER_STAGE_GEOMETRY_BIT; }
  if (stages & ShaderStage::TessControl)    { result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT; }
  if (stages & ShaderStage::TessEvaluation) { result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; }
  if (stages & ShaderStage::Fragment)       { result |= VK_SHADER_STAGE_FRAGMENT_BIT; }
  if (stages & ShaderStage::Compute)        { result |= VK_SHADER_STAGE_COMPUTE_BIT; }
  if (stages & ShaderStage::Task)           { result |= VK_SHADER_STAGE_TASK_BIT_EXT; }
  if (stages & ShaderStage::Mesh)           { result |= VK_SHADER_STAGE_MESH_BIT_EXT; }
  if (stages & ShaderStage::RayGen)         { result |= VK_SHADER_STAGE_RAYGEN_BIT_KHR; }
  if (stages & ShaderStage::AnyHit)         { result |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR; }
  if (stages & ShaderStage::ClosestHit)     { result |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR; }
  if (stages & ShaderStage::Miss)           { result |= VK_SHADER_STAGE_MISS_BIT_KHR; }
  if (stages & ShaderStage::Intersection)   { result |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR; }
  return static_cast<VkShaderStageFlagBits>(result);
  }
constexpr inline VkPresentModeKHR        translate(hlgl::VsyncMode mode) {
  switch (mode) {
    case hlgl::VsyncMode::Immediate:   return VK_PRESENT_MODE_IMMEDIATE_KHR;
    case hlgl::VsyncMode::Fifo:        return VK_PRESENT_MODE_FIFO_KHR;
    case hlgl::VsyncMode::FifoRelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    case hlgl::VsyncMode::Mailbox:     return VK_PRESENT_MODE_MAILBOX_KHR;
  }
  }
constexpr inline VkSamplerAddressMode    translate(hlgl::WrapMode mode) {
  switch (mode) {
  case hlgl::WrapMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case hlgl::WrapMode::MirrorRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  case hlgl::WrapMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case hlgl::WrapMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  case hlgl::WrapMode::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
  default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
}

constexpr inline size_t                  bytesPerPixel(hlgl::ImageFormat format) {
  switch (format) {
  case hlgl::ImageFormat::RG4i:        return 1;
  case hlgl::ImageFormat::RGBA4i:      return 2;
  case hlgl::ImageFormat::R5G6B5i:     return 2;
  case hlgl::ImageFormat::RGB5A1i:     return 2;
  case hlgl::ImageFormat::RGB8i:       return 3;
  case hlgl::ImageFormat::RGB8i_srgb:  return 3;
  case hlgl::ImageFormat::RGBA8i:      return 4;
  case hlgl::ImageFormat::RGBA8i_srgb: return 4;
  case hlgl::ImageFormat::BGR8i:       return 3;
  case hlgl::ImageFormat::BGR8i_srgb:  return 3;
  case hlgl::ImageFormat::BGRA8i:      return 4;
  case hlgl::ImageFormat::BGRA8i_srgb: return 4;
  case hlgl::ImageFormat::R16i:        return 2;
  case hlgl::ImageFormat::RG16i:       return 4;
  case hlgl::ImageFormat::RGB16i:      return 6;
  case hlgl::ImageFormat::RGBA16i:     return 8;
  case hlgl::ImageFormat::R16f:        return 2;
  case hlgl::ImageFormat::RG16f:       return 4;
  case hlgl::ImageFormat::RGB16f:      return 6;
  case hlgl::ImageFormat::RGBA16f:     return 8;
  case hlgl::ImageFormat::R32i:        return 4;
  case hlgl::ImageFormat::RG32i:       return 8;
  case hlgl::ImageFormat::RGB32i:      return 12;
  case hlgl::ImageFormat::RGBA32i:     return 16;
  case hlgl::ImageFormat::R32f:        return 4;
  case hlgl::ImageFormat::RG32f:       return 8;
  case hlgl::ImageFormat::RGB32f:      return 12;
  case hlgl::ImageFormat::RGBA32f:     return 16;
  case hlgl::ImageFormat::A2RGB10i:    return 4;
  case hlgl::ImageFormat::B10GR11f:    return 4;
  case hlgl::ImageFormat::D24S8:       return 4;
  case hlgl::ImageFormat::D32f:        return 4;
  case hlgl::ImageFormat::D32fS8:      return 8;
  default: return 0;
  }
  }
constexpr inline bool                    isHdrSurfaceFormat(VkFormat format) {
  switch (format) {
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
      return true;
    default:
      return false;
  }
  }

} // namespace hlgl

#define DEBUG_OBJCREATION(fmt,...)  hlgl::debugPrint(hlgl::DebugSeverity::ObjectCreation, fmt, __VA_ARGS__)
#define DEBUG_VERBOSE(fmt,...)      hlgl::debugPrint(hlgl::DebugSeverity::Verbose,        fmt, __VA_ARGS__)
#define DEBUG_INFO(fmt,...)         hlgl::debugPrint(hlgl::DebugSeverity::Info,           fmt, __VA_ARGS__)
#define DEBUG_WARNING(fmt,...)      hlgl::debugPrint(hlgl::DebugSeverity::Warning,        fmt, __VA_ARGS__)
#define DEBUG_ERROR(fmt,...)        hlgl::debugPrint(hlgl::DebugSeverity::Error,          fmt, __VA_ARGS__)
#define DEBUG_FATAL(fmt,...)        hlgl::debugPrint(hlgl::DebugSeverity::Fatal,          fmt, __VA_ARGS__)

// Check the VkResult returned by an expression, prints an error on failure.
#define VKCHECK(expr)      hlgl::checkVkResult(expr, #expr, hlgl::DebugSeverity::Error)

// Check the VkResult returned by an expression, prints a warning on failure.
#define VKCHECK_WARN(expr) hlgl::checkVkResult(expr, #expr, hlgl::DebugSeverity::Warning)

// Check the VkResult returned by an expression, prints an error on failure.
// Does not consider VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR to be errors.
#define VKCHECK_SWAPCHAIN(expr)      hlgl::checkVkResult(expr, #expr, hlgl::DebugSeverity::Error, true)

#endif // HLGL_VK_VULKAN_HEADERS_H