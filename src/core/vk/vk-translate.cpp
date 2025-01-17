#include "vk-translate.h"

VkIndexType hlgl::translateIndexType(uint32_t size) {
  switch (size) {
  case 1: return VK_INDEX_TYPE_UINT8_EXT;
  case 2: return VK_INDEX_TYPE_UINT16;
  case 4: return VK_INDEX_TYPE_UINT32;
  default: return VK_INDEX_TYPE_NONE_KHR;
  }
}

VkBlendFactor hlgl::translate(hlgl::BlendFactor factor) {
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

VkBlendOp hlgl::translate(hlgl::BlendOp op) {
  switch (op) {
  case hlgl::BlendOp::Add:              return VK_BLEND_OP_ADD;
  case hlgl::BlendOp::Subtract:         return VK_BLEND_OP_SUBTRACT;
  case hlgl::BlendOp::SubtractReverse:  return VK_BLEND_OP_REVERSE_SUBTRACT;
  case hlgl::BlendOp::Max:              return VK_BLEND_OP_MAX;
  case hlgl::BlendOp::Min:              return VK_BLEND_OP_MIN;
  default: return VK_BLEND_OP_ADD;
  }
}

VkFormat hlgl::translate(hlgl::Format format) {
  switch (format) {
  case hlgl::Format::RG4i:         return VK_FORMAT_R4G4_UNORM_PACK8;
  case hlgl::Format::RGBA4i:      return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
  case hlgl::Format::R5G6B5i:     return VK_FORMAT_R5G6B5_UNORM_PACK16;
  case hlgl::Format::RGB5A1i:     return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
  case hlgl::Format::RGB8i:       return VK_FORMAT_R8G8B8_UNORM;
  case hlgl::Format::RGB8i_srgb:  return VK_FORMAT_R8G8B8_SRGB;
  case hlgl::Format::RGBA8i:      return VK_FORMAT_R8G8B8A8_UNORM;
  case hlgl::Format::RGBA8i_srgb: return VK_FORMAT_R8G8B8A8_SRGB;
  case hlgl::Format::BGR8i:       return VK_FORMAT_B8G8R8_UNORM;
  case hlgl::Format::BGR8i_srgb:  return VK_FORMAT_B8G8R8_SRGB;
  case hlgl::Format::BGRA8i:      return VK_FORMAT_B8G8R8A8_UNORM;
  case hlgl::Format::BGRA8i_srgb: return VK_FORMAT_B8G8R8A8_SRGB;
  case hlgl::Format::R16i:        return VK_FORMAT_R16_UNORM;
  case hlgl::Format::RG16i:       return VK_FORMAT_R16G16_UNORM;
  case hlgl::Format::RGB16i:      return VK_FORMAT_R16G16B16_UNORM;
  case hlgl::Format::RGBA16i:     return VK_FORMAT_R16G16B16A16_UNORM;
  case hlgl::Format::R16f:        return VK_FORMAT_R16_SFLOAT;
  case hlgl::Format::RG16f:       return VK_FORMAT_R16G16_SFLOAT;
  case hlgl::Format::RGB16f:      return VK_FORMAT_R16G16B16_SFLOAT;
  case hlgl::Format::RGBA16f:     return VK_FORMAT_R16G16B16A16_SFLOAT;
  case hlgl::Format::R32i:        return VK_FORMAT_R32_UINT;
  case hlgl::Format::RG32i:       return VK_FORMAT_R32G32_UINT;
  case hlgl::Format::RGB32i:      return VK_FORMAT_R32G32B32_UINT;
  case hlgl::Format::RGBA32i:     return VK_FORMAT_R32G32B32A32_UINT;
  case hlgl::Format::R32f:        return VK_FORMAT_R32_SFLOAT;
  case hlgl::Format::RG32f:       return VK_FORMAT_R32G32_SFLOAT;
  case hlgl::Format::RGB32f:      return VK_FORMAT_R32G32B32_SFLOAT;
  case hlgl::Format::RGBA32f:     return VK_FORMAT_R32G32B32A32_SFLOAT;
  case hlgl::Format::A2RGB10i:    return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
  case hlgl::Format::B10RG11f:    return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
  case hlgl::Format::D24S8:       return VK_FORMAT_D24_UNORM_S8_UINT;
  case hlgl::Format::D32f:        return VK_FORMAT_D32_SFLOAT;
  case hlgl::Format::D32fS8:      return VK_FORMAT_D32_SFLOAT_S8_UINT;
  default: return VK_FORMAT_UNDEFINED;
  }
}

hlgl::Format hlgl::translate(VkFormat format) {
  switch (format) {
  case VK_FORMAT_R4G4_UNORM_PACK8:         return hlgl::Format::RG4i;
  case VK_FORMAT_R4G4B4A4_UNORM_PACK16:    return hlgl::Format::RGBA4i;
  case VK_FORMAT_R5G6B5_UNORM_PACK16:      return hlgl::Format::R5G6B5i;
  case VK_FORMAT_R5G5B5A1_UNORM_PACK16:    return hlgl::Format::RGB5A1i;
  case VK_FORMAT_R8G8B8_UNORM:             return hlgl::Format::RGB8i;
  case VK_FORMAT_R8G8B8_SRGB:              return hlgl::Format::RGB8i_srgb;
  case VK_FORMAT_R8G8B8A8_UNORM:           return hlgl::Format::RGBA8i;
  case VK_FORMAT_R8G8B8A8_SRGB:            return hlgl::Format::RGBA8i_srgb;
  case VK_FORMAT_B8G8R8_UNORM:             return hlgl::Format::BGR8i;
  case VK_FORMAT_B8G8R8_SRGB:              return hlgl::Format::BGR8i_srgb;
  case VK_FORMAT_B8G8R8A8_UNORM:           return hlgl::Format::BGRA8i;
  case VK_FORMAT_B8G8R8A8_SRGB:            return hlgl::Format::BGRA8i_srgb;
  case VK_FORMAT_R16_UNORM:                return hlgl::Format::R16i;
  case VK_FORMAT_R16G16_UNORM:             return hlgl::Format::RG16i;
  case VK_FORMAT_R16G16B16_UNORM:          return hlgl::Format::RGB16i;
  case VK_FORMAT_R16G16B16A16_UNORM:       return hlgl::Format::RGBA16i;
  case VK_FORMAT_R16_SFLOAT:               return hlgl::Format::R16f;
  case VK_FORMAT_R16G16_SFLOAT:            return hlgl::Format::RG16f;
  case VK_FORMAT_R16G16B16_SFLOAT:         return hlgl::Format::RGB16f;
  case VK_FORMAT_R16G16B16A16_SFLOAT:      return hlgl::Format::RGBA16f;
  case VK_FORMAT_R32_UINT:                 return hlgl::Format::R32i;
  case VK_FORMAT_R32G32_UINT:              return hlgl::Format::RG32i;
  case VK_FORMAT_R32G32B32_UINT:           return hlgl::Format::RGB32i;
  case VK_FORMAT_R32G32B32A32_UINT:        return hlgl::Format::RGBA32i;
  case VK_FORMAT_R32_SFLOAT:               return hlgl::Format::R32f;
  case VK_FORMAT_R32G32_SFLOAT:            return hlgl::Format::RG32f;
  case VK_FORMAT_R32G32B32_SFLOAT:         return hlgl::Format::RGB32f;
  case VK_FORMAT_R32G32B32A32_SFLOAT:      return hlgl::Format::RGBA32f;
  case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return hlgl::Format::A2RGB10i;
  case VK_FORMAT_B10G11R11_UFLOAT_PACK32:  return hlgl::Format::B10RG11f;
  case VK_FORMAT_D24_UNORM_S8_UINT:        return hlgl::Format::D24S8;
  case VK_FORMAT_D32_SFLOAT:               return hlgl::Format::D32f;
  case VK_FORMAT_D32_SFLOAT_S8_UINT:       return hlgl::Format::D32fS8;
  default: return hlgl::Format::Undefined;
  }
}

VkImageAspectFlags hlgl::translateAspect(hlgl::Format format) {
  switch (format) {
  case hlgl::Format::D32f:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  case hlgl::Format::D24S8:
  case hlgl::Format::D32fS8:
    return (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
  default:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

VkImageAspectFlags hlgl::translateAspect(VkFormat format) {
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

VkSamplerAddressMode hlgl::translate(hlgl::WrapMode mode) {
  switch (mode) {
  case hlgl::WrapMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case hlgl::WrapMode::MirrorRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  case hlgl::WrapMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case hlgl::WrapMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  case hlgl::WrapMode::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
  default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
}

VkFilter hlgl::translate(hlgl::FilterMode mode) {
  switch (mode) {
  case hlgl::FilterMode::Nearest: return VK_FILTER_NEAREST;
  case hlgl::FilterMode::Linear: return VK_FILTER_LINEAR;
  case hlgl::FilterMode::Min: return VK_FILTER_LINEAR;
  case hlgl::FilterMode::Max: return VK_FILTER_LINEAR;
  default: return VK_FILTER_NEAREST;
  }
}

VkSamplerMipmapMode hlgl::translateMipMode(hlgl::FilterMode mode) {
  switch (mode) {
  case hlgl::FilterMode::Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  case hlgl::FilterMode::Linear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  case hlgl::FilterMode::Min: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  case hlgl::FilterMode::Max: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  default: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }
}

VkSamplerReductionMode hlgl::translateReduction(hlgl::FilterMode mode) {
  switch (mode) {
  case hlgl::FilterMode::Min: return VK_SAMPLER_REDUCTION_MODE_MIN;
  case hlgl::FilterMode::Max: return VK_SAMPLER_REDUCTION_MODE_MAX;
  default: return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
  }
}

VkFrontFace hlgl::translate(hlgl::FrontFace mode) {
  switch (mode) {
  case hlgl::FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  case hlgl::FrontFace::Clockwise: return VK_FRONT_FACE_CLOCKWISE;
  default: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  }
}

VkPrimitiveTopology hlgl::translate(hlgl::Primitive type) {
  switch (type) {
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

VkCullModeFlags hlgl::translate(hlgl::CullMode mode) {
  switch (mode) {
  case hlgl::CullMode::None: return VK_CULL_MODE_NONE;
  case hlgl::CullMode::Back: return VK_CULL_MODE_BACK_BIT;
  case hlgl::CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
  case hlgl::CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
  default: return VK_CULL_MODE_NONE;
  }
}

VkSampleCountFlagBits hlgl::translateMsaa(uint32_t amount) {
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

VkCompareOp hlgl::translate(hlgl::CompareOp op) {
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