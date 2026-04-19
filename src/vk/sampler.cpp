#include "sampler.h"
#include "context.h"
#include "debug.h"
#include "vulkan-translate.h"

hlgl::Sampler::Sampler(CreateSamplerParams&& params) {
  if (params.wrapU == WrapMode::DontCare)
    params.wrapU = params.wrapping;
  if (params.wrapV == WrapMode::DontCare)
    params.wrapV = params.wrapping;
  if (params.wrapW == WrapMode::DontCare)
    params.wrapW = params.wrapping;

  if (params.filterMin == FilterMode::DontCare)
    params.filterMin = params.filtering;
  if (params.filterMag == FilterMode::DontCare)
    params.filterMag = params.filtering;
  if (params.filterMips == FilterMode::DontCare)
    params.filterMips = (params.maxLod > 1.0f) ? params.filtering : FilterMode::Nearest;

  VkSamplerCustomBorderColorCreateInfoEXT bci {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT
  };
  VkSamplerReductionModeCreateInfo rci {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
    .reductionMode = translateReduction(params.filtering)
  };
  VkSamplerCreateInfo sci {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = &rci,
    .magFilter = translate(params.filterMag),
    .minFilter = translate(params.filterMin),
    .mipmapMode = translateMipMode(params.filterMips),
    .addressModeU = translate(params.wrapU),
    .addressModeV = translate(params.wrapV),
    .addressModeW = translate(params.wrapW),
    .anisotropyEnable = (params.maxAnisotropy > 1.0f) ? true : false,
    .maxAnisotropy = params.maxAnisotropy,
    .maxLod = params.maxLod
  };
  if (params.borderColor == ColorRGBAi{0,0,0,0} )
    sci.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
  else if (params.borderColor == ColorRGBAi{0,0,0,255} )
    sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  else if (params.borderColor == ColorRGBAi{255,255,255,255} )
    sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
  else {
    sci.borderColor = VK_BORDER_COLOR_INT_CUSTOM_EXT;
    bci.customBorderColor.int32[0] = params.borderColor[0];
    bci.customBorderColor.int32[1] = params.borderColor[1];
    bci.customBorderColor.int32[2] = params.borderColor[2];
    bci.customBorderColor.int32[3] = params.borderColor[3];
    bci.format = VK_FORMAT_UNDEFINED;
    rci.pNext = &bci;
  }
  if (!VKCHECK(vkCreateSampler(getDevice(), &sci, nullptr, &sampler)) || !sampler) {
    debugPrint(DebugSeverity::Error, "Failed to create image sampler.");
    return;
  }

  if (params.debugName && isValidationEnabled()) {
    VkDebugUtilsObjectNameInfoEXT info { 
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = VK_OBJECT_TYPE_SAMPLER,
      .objectHandle = (uint64_t)sampler,
      .pObjectName = params.debugName,
    };
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(getDevice(), &info)))
      debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", params.debugName));
  }
}

hlgl::Sampler::~Sampler() {
  if (sampler) {
    queueDeletion(DelQueueSampler{sampler});
    sampler = nullptr;
  }
}

hlgl::Sampler* hlgl::createSampler(CreateSamplerParams params) {
  Sampler* result = new Sampler(std::move(params));
  return (result && result->isValid()) ? result : nullptr;
}

hlgl::SamplerShared hlgl::createSamplerShared(CreateSamplerParams params) {
  SamplerShared result = std::make_shared<Sampler>(std::move(params));
  return (result && result->isValid()) ? std::move(result) : nullptr;
}

hlgl::SamplerUnique hlgl::createSamplerUnique(CreateSamplerParams params) {
  SamplerUnique result = std::make_unique<Sampler>(std::move(params));
  return (result && result->isValid()) ? std::move(result) : nullptr;
}

void hlgl::destroySampler(Sampler* sampler) {
  delete sampler;
}