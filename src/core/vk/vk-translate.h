#pragma once

#include "vk-includes.h"
#include <hlgl/core/types.h>

namespace hlgl {

VkImageAspectFlags      translateAspect(hlgl::Format format);
VkImageAspectFlags      translateAspect(VkFormat format);
VkIndexType             translateIndexType(uint32_t size);
VkSampleCountFlagBits   translateMsaa(uint32_t samples);
VkSamplerMipmapMode     translateMipMode(hlgl::FilterMode mode);
VkSamplerReductionMode  translateReduction(hlgl::FilterMode mode);

VkBlendFactor           translate(hlgl::BlendFactor factor);
VkBlendOp               translate(hlgl::BlendOp op);
VkCompareOp             translate(hlgl::CompareOp op);
VkCullModeFlags         translate(hlgl::CullMode mode);
VkFilter                translate(hlgl::FilterMode mode);
VkFormat                translate(hlgl::Format format);
hlgl::Format            translate(VkFormat format);
VkFrontFace             translate(hlgl::FrontFace front);
VkPrimitiveTopology     translate(hlgl::Primitive mode);
VkSamplerAddressMode    translate(hlgl::WrapMode mode);

size_t                  bytesPerPixel(hlgl::Format format);

} // namespace hlgl