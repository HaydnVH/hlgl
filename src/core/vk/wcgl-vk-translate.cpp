#include "wcgl-vk-translate.h"

VkIndexType wcgl::translateIndexType(uint32_t size) {
  switch (size) {
  case 1: return VK_INDEX_TYPE_UINT8_EXT;
  case 2: return VK_INDEX_TYPE_UINT16;
  case 4: return VK_INDEX_TYPE_UINT32;
  default: return VK_INDEX_TYPE_NONE_KHR;
  }
}

VkBlendFactor wcgl::translate(wcgl::BlendFactor factor) {
  switch (factor) {
  case wcgl::BlendFactor::Zero:               return VK_BLEND_FACTOR_ZERO;
  case wcgl::BlendFactor::One:                return VK_BLEND_FACTOR_ONE;
  case wcgl::BlendFactor::SrcColor:           return VK_BLEND_FACTOR_SRC_COLOR;
  case wcgl::BlendFactor::OneMinusSrcColor:   return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  case wcgl::BlendFactor::DstColor:           return VK_BLEND_FACTOR_DST_COLOR;
  case wcgl::BlendFactor::OneMinusDstColor:   return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  case wcgl::BlendFactor::SrcAlpha:           return VK_BLEND_FACTOR_SRC_ALPHA;
  case wcgl::BlendFactor::OneMinusSrcAlpha:   return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  case wcgl::BlendFactor::DstAlpha:           return VK_BLEND_FACTOR_DST_ALPHA;
  case wcgl::BlendFactor::OneMinusDstAlpha:   return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  case wcgl::BlendFactor::ConstColor:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
  case wcgl::BlendFactor::OneMinusConstColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
  case wcgl::BlendFactor::ConstAlpha:         return VK_BLEND_FACTOR_CONSTANT_ALPHA;
  case wcgl::BlendFactor::OneMinusConstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
  case wcgl::BlendFactor::SrcAlphaSaturate:   return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
  case wcgl::BlendFactor::Src1Color:          return VK_BLEND_FACTOR_SRC1_COLOR;
  case wcgl::BlendFactor::OneMinusSrc1Color:  return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
  case wcgl::BlendFactor::Src1Alpha:          return VK_BLEND_FACTOR_SRC1_ALPHA;
  case wcgl::BlendFactor::OneMinusSrc1Alpha:  return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  default: return VK_BLEND_FACTOR_ZERO;
  }
}

VkBlendOp wcgl::translate(wcgl::BlendOp op) {
  switch (op) {
  case wcgl::BlendOp::Add:              return VK_BLEND_OP_ADD;
  case wcgl::BlendOp::Subtract:         return VK_BLEND_OP_SUBTRACT;
  case wcgl::BlendOp::SubtractReverse:  return VK_BLEND_OP_REVERSE_SUBTRACT;
  case wcgl::BlendOp::Max:              return VK_BLEND_OP_MAX;
  case wcgl::BlendOp::Min:              return VK_BLEND_OP_MIN;
  default: return VK_BLEND_OP_ADD;
  }
}

VkFormat wcgl::translate(wcgl::Format format) {
  switch (format) {
  case wcgl::Format::RG4i:         return VK_FORMAT_R4G4_UNORM_PACK8;
  case wcgl::Format::RGBA4i:      return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
  case wcgl::Format::R5G6B5i:     return VK_FORMAT_R5G6B5_UNORM_PACK16;
  case wcgl::Format::RGB5A1i:     return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
  case wcgl::Format::RGB8i:       return VK_FORMAT_R8G8B8_UNORM;
  case wcgl::Format::RGB8i_srgb:  return VK_FORMAT_R8G8B8_SRGB;
  case wcgl::Format::RGBA8i:      return VK_FORMAT_R8G8B8A8_UNORM;
  case wcgl::Format::RGBA8i_srgb: return VK_FORMAT_R8G8B8A8_SRGB;
  case wcgl::Format::BGR8i:       return VK_FORMAT_B8G8R8_UNORM;
  case wcgl::Format::BGR8i_srgb:  return VK_FORMAT_B8G8R8_SRGB;
  case wcgl::Format::BGRA8i:      return VK_FORMAT_B8G8R8A8_UNORM;
  case wcgl::Format::BGRA8i_srgb: return VK_FORMAT_B8G8R8A8_SRGB;
  case wcgl::Format::R16i:        return VK_FORMAT_R16_UNORM;
  case wcgl::Format::RG16i:       return VK_FORMAT_R16G16_UNORM;
  case wcgl::Format::RGB16i:      return VK_FORMAT_R16G16B16_UNORM;
  case wcgl::Format::RGBA16i:     return VK_FORMAT_R16G16B16A16_UNORM;
  case wcgl::Format::R16f:        return VK_FORMAT_R16_SFLOAT;
  case wcgl::Format::RG16f:       return VK_FORMAT_R16G16_SFLOAT;
  case wcgl::Format::RGB16f:      return VK_FORMAT_R16G16B16_SFLOAT;
  case wcgl::Format::RGBA16f:     return VK_FORMAT_R16G16B16A16_SFLOAT;
  case wcgl::Format::R32i:        return VK_FORMAT_R32_UINT;
  case wcgl::Format::RG32i:       return VK_FORMAT_R32G32_UINT;
  case wcgl::Format::RGB32i:      return VK_FORMAT_R32G32B32_UINT;
  case wcgl::Format::RGBA32i:     return VK_FORMAT_R32G32B32A32_UINT;
  case wcgl::Format::R32f:        return VK_FORMAT_R32_SFLOAT;
  case wcgl::Format::RG32f:       return VK_FORMAT_R32G32_SFLOAT;
  case wcgl::Format::RGB32f:      return VK_FORMAT_R32G32B32_SFLOAT;
  case wcgl::Format::RGBA32f:     return VK_FORMAT_R32G32B32A32_SFLOAT;
  case wcgl::Format::A2RGB10i:    return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
  case wcgl::Format::B10RG11f:    return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
  case wcgl::Format::D24S8:       return VK_FORMAT_D24_UNORM_S8_UINT;
  case wcgl::Format::D32f:        return VK_FORMAT_D32_SFLOAT;
  case wcgl::Format::D32fS8:      return VK_FORMAT_D32_SFLOAT_S8_UINT;
  default: return VK_FORMAT_UNDEFINED;
  }
}

wcgl::Format wcgl::translate(VkFormat format) {
  switch (format) {
  case VK_FORMAT_R4G4_UNORM_PACK8:         return wcgl::Format::RG4i;
  case VK_FORMAT_R4G4B4A4_UNORM_PACK16:    return wcgl::Format::RGBA4i;
  case VK_FORMAT_R5G6B5_UNORM_PACK16:      return wcgl::Format::R5G6B5i;
  case VK_FORMAT_R5G5B5A1_UNORM_PACK16:    return wcgl::Format::RGB5A1i;
  case VK_FORMAT_R8G8B8_UNORM:             return wcgl::Format::RGB8i;
  case VK_FORMAT_R8G8B8_SRGB:              return wcgl::Format::RGB8i_srgb;
  case VK_FORMAT_R8G8B8A8_UNORM:           return wcgl::Format::RGBA8i;
  case VK_FORMAT_R8G8B8A8_SRGB:            return wcgl::Format::RGBA8i_srgb;
  case VK_FORMAT_B8G8R8_UNORM:             return wcgl::Format::BGR8i;
  case VK_FORMAT_B8G8R8_SRGB:              return wcgl::Format::BGR8i_srgb;
  case VK_FORMAT_B8G8R8A8_UNORM:           return wcgl::Format::BGRA8i;
  case VK_FORMAT_B8G8R8A8_SRGB:            return wcgl::Format::BGRA8i_srgb;
  case VK_FORMAT_R16_UNORM:                return wcgl::Format::R16i;
  case VK_FORMAT_R16G16_UNORM:             return wcgl::Format::RG16i;
  case VK_FORMAT_R16G16B16_UNORM:          return wcgl::Format::RGB16i;
  case VK_FORMAT_R16G16B16A16_UNORM:       return wcgl::Format::RGBA16i;
  case VK_FORMAT_R16_SFLOAT:               return wcgl::Format::R16f;
  case VK_FORMAT_R16G16_SFLOAT:            return wcgl::Format::RG16f;
  case VK_FORMAT_R16G16B16_SFLOAT:         return wcgl::Format::RGB16f;
  case VK_FORMAT_R16G16B16A16_SFLOAT:      return wcgl::Format::RGBA16f;
  case VK_FORMAT_R32_UINT:                 return wcgl::Format::R32i;
  case VK_FORMAT_R32G32_UINT:              return wcgl::Format::RG32i;
  case VK_FORMAT_R32G32B32_UINT:           return wcgl::Format::RGB32i;
  case VK_FORMAT_R32G32B32A32_UINT:        return wcgl::Format::RGBA32i;
  case VK_FORMAT_R32_SFLOAT:               return wcgl::Format::R32f;
  case VK_FORMAT_R32G32_SFLOAT:            return wcgl::Format::RG32f;
  case VK_FORMAT_R32G32B32_SFLOAT:         return wcgl::Format::RGB32f;
  case VK_FORMAT_R32G32B32A32_SFLOAT:      return wcgl::Format::RGBA32f;
  case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return wcgl::Format::A2RGB10i;
  case VK_FORMAT_B10G11R11_UFLOAT_PACK32:  return wcgl::Format::B10RG11f;
  case VK_FORMAT_D24_UNORM_S8_UINT:        return wcgl::Format::D24S8;
  case VK_FORMAT_D32_SFLOAT:               return wcgl::Format::D32f;
  case VK_FORMAT_D32_SFLOAT_S8_UINT:       return wcgl::Format::D32fS8;
  default: return wcgl::Format::Undefined;
  }
}

VkImageAspectFlags wcgl::translateAspect(wcgl::Format format) {
  switch (format) {
  case wcgl::Format::D32f:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  case wcgl::Format::D24S8:
  case wcgl::Format::D32fS8:
    return (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
  default:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

VkImageAspectFlags wcgl::translateAspect(VkFormat format) {
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

VkSamplerAddressMode wcgl::translate(wcgl::WrapMode mode) {
  switch (mode) {
  case wcgl::WrapMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case wcgl::WrapMode::MirrorRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  case wcgl::WrapMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case wcgl::WrapMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  case wcgl::WrapMode::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
  default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
}

VkFilter wcgl::translate(wcgl::FilterMode mode) {
  switch (mode) {
  case wcgl::FilterMode::Nearest: return VK_FILTER_NEAREST;
  case wcgl::FilterMode::Linear: return VK_FILTER_LINEAR;
  case wcgl::FilterMode::Min: return VK_FILTER_LINEAR;
  case wcgl::FilterMode::Max: return VK_FILTER_LINEAR;
  default: return VK_FILTER_NEAREST;
  }
}

VkSamplerMipmapMode wcgl::translateMipMode(wcgl::FilterMode mode) {
  switch (mode) {
  case wcgl::FilterMode::Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  case wcgl::FilterMode::Linear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  case wcgl::FilterMode::Min: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  case wcgl::FilterMode::Max: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  default: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }
}

VkSamplerReductionMode wcgl::translateReduction(wcgl::FilterMode mode) {
  switch (mode) {
  case wcgl::FilterMode::Min: return VK_SAMPLER_REDUCTION_MODE_MIN;
  case wcgl::FilterMode::Max: return VK_SAMPLER_REDUCTION_MODE_MAX;
  default: return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
  }
}

VkFrontFace wcgl::translate(wcgl::FrontFace mode) {
  switch (mode) {
  case wcgl::FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  case wcgl::FrontFace::Clockwise: return VK_FRONT_FACE_CLOCKWISE;
  default: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  }
}

VkPrimitiveTopology wcgl::translate(wcgl::Primitive type) {
  switch (type) {
  case wcgl::Primitive::Points:  return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  case wcgl::Primitive::Lines:  return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  case wcgl::Primitive::LineStrip:  return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  case wcgl::Primitive::Triangles:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case wcgl::Primitive::TriangleStrip:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  case wcgl::Primitive::TriangleFan:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
  case wcgl::Primitive::LinesWithAdj:  return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
  case wcgl::Primitive::LineStripWithAdj:  return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
  case wcgl::Primitive::TrianglesWithAdj:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
  case wcgl::Primitive::TriangleStripWithAdj:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
  case wcgl::Primitive::Patches:  return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
  default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }
}

VkCullModeFlags wcgl::translate(wcgl::CullMode mode) {
  switch (mode) {
  case wcgl::CullMode::None: return VK_CULL_MODE_NONE;
  case wcgl::CullMode::Back: return VK_CULL_MODE_BACK_BIT;
  case wcgl::CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
  case wcgl::CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
  default: return VK_CULL_MODE_NONE;
  }
}

VkSampleCountFlagBits wcgl::translateMsaa(uint32_t amount) {
  switch (amount) {
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

VkCompareOp wcgl::translate(wcgl::CompareOp op) {
  switch (op) {
  case wcgl::CompareOp::Less: return VK_COMPARE_OP_LESS;
  case wcgl::CompareOp::Greater: return VK_COMPARE_OP_GREATER;
  case wcgl::CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
  case wcgl::CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
  case wcgl::CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
  case wcgl::CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
  case wcgl::CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
  case wcgl::CompareOp::Never: return VK_COMPARE_OP_NEVER;
  default: return VK_COMPARE_OP_NEVER;
  }
}