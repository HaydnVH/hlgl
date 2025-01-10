#pragma once

#include "wcgl-vk-includes.h"
#include <wcgl/core/wcgl-types.h>

namespace wcgl {

VkImageAspectFlags      translateAspect(wcgl::Format format);
VkImageAspectFlags      translateAspect(VkFormat format);
VkIndexType             translateIndexType(uint32_t size);
VkSampleCountFlagBits   translateMsaa(uint32_t samples);
VkSamplerMipmapMode     translateMipMode(wcgl::FilterMode mode);
VkSamplerReductionMode  translateReduction(wcgl::FilterMode mode);

VkBlendFactor           translate(wcgl::BlendFactor factor);
VkBlendOp               translate(wcgl::BlendOp op);
VkCompareOp             translate(wcgl::CompareOp op);
VkCullModeFlags         translate(wcgl::CullMode mode);
VkFilter                translate(wcgl::FilterMode mode);
VkFormat                translate(wcgl::Format format);
wcgl::Format            translate(VkFormat format);
VkFrontFace             translate(wcgl::FrontFace front);
VkPrimitiveTopology     translate(wcgl::Primitive mode);
VkSamplerAddressMode    translate(wcgl::WrapMode mode);

} // namespace wcgl