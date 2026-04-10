#ifndef HLGL_VK_TRANSLATE_H
#define HLGL_VK_TRANSLATE_H

#include "vkimpl-includes.h"
#include <hlgl/types.h>

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
VkPresentModeKHR        translate(hlgl::VsyncMode mode);
VkSamplerAddressMode    translate(hlgl::WrapMode mode);

size_t                  bytesPerPixel(hlgl::Format format);
bool                    isHdrSurfaceFormat(VkFormat format);

} // namespace hlgl

#endif // HLGL_VK_TRANSLATE_H