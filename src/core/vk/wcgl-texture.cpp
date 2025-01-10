#include "wcgl-vk-includes.h"
#include "wcgl-vk-debug.h"
#include "wcgl-vk-translate.h"
#include "wcgl/core/wcgl-context.h"
#include "wcgl/core/wcgl-texture.h"

wcgl::Texture::Texture(const Context& context, TextureParams params, VkImage existingImage)
: context_(context)
{
  if (params.eWrapU == WrapMode::DontCare)
    params.eWrapU = params.eWrapping;
  if (params.eWrapV == WrapMode::DontCare)
    params.eWrapV = params.eWrapping;
  if (params.eWrapW == WrapMode::DontCare)
    params.eWrapW = params.eWrapping;

  if (params.eFilterMin == FilterMode::DontCare)
    params.eFilterMin = params.eFiltering;
  if (params.eFilterMag == FilterMode::DontCare)
    params.eFilterMag = params.eFiltering;
  if (params.eFilterMips == FilterMode::DontCare)
    params.eFilterMips = (params.iMipCount > 1) ? params.eFiltering : FilterMode::Nearest;

  if (params.bMatchDisplaySize) {
    std::tie(params.iWidth, params.iHeight) = context.getDisplaySize();
    params.iDepth = 1;
    // TODO: Register a callback so this texture can be resized when the swapchain is resized.
    // Alternatively, make it the size of the screen rather than the window so it never needs to be resized.
  }

  if (params.iWidth == 0 || params.iHeight == 0 || params.iDepth == 0) {
    debugPrint(DebugSeverity::Error, "Texture must have non-zero dimensions.");
    return;
  }

  if (params.iMipCount == 0) {
    debugPrint(DebugSeverity::Error, "Texture must have non-zero mip count.");
    return;
  }

  if (params.eFormat == Format::Undefined) {
    debugPrint(DebugSeverity::Error, "Texture must have a defined format.");
    return;
  }

  extent_ = VkExtent3D{params.iWidth, params.iHeight, params.iDepth};
  mipIndex_ = params.iMipBase;
  mipCount_ = params.iMipCount;
  format_ = translate(params.eFormat);

  // TODO: Figure out usage flags.
  VkImageUsageFlags usage {0};
  if (params.eUsage & TextureUsage::Framebuffer) {
    usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  if (params.eUsage & TextureUsage::Sampler) {
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (params.eUsage & TextureUsage::Storage) {
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  }

  if (existingImage) {
    image_ = existingImage;
  }
  else {
    VkImageCreateInfo ici {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = (params.iDepth > 1) ? VK_IMAGE_TYPE_3D : (params.iHeight > 1) ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D,
      .format = format_,
      .extent = extent_,
      .mipLevels = mipCount_,
      .arrayLayers = params.iLayerCount,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo aci { .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE };
    if (!VKCHECK(vmaCreateImage(context_.allocator_, &ici, &aci, &image_, &allocation_, nullptr)) || !image_) {
      debugPrint(DebugSeverity::Error, "Failed to create image.");
      return;
    }
  }

  VkImageViewCreateInfo vci {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = image_,
    .viewType = (params.iDepth > 1) ? VK_IMAGE_VIEW_TYPE_3D : (params.iHeight > 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_1D,
    .format = format_,
    .subresourceRange = {
      .aspectMask = translateAspect(format_),
      .baseMipLevel = mipIndex_,
      .levelCount = mipCount_,
      .baseArrayLayer = params.iLayerBase,
      .layerCount = params.iLayerCount } };
  if (!VKCHECK(vkCreateImageView(context_.device_, &vci, nullptr, &view_)) || !view_) {
    debugPrint(DebugSeverity::Error, "Failed to create image view.");
    if (allocation_) {
      vmaDestroyImage(context_.allocator_, image_, allocation_);
      image_ = nullptr;
      allocation_ = nullptr;
    }
    return;
  }

  // Create the sampler.
  if (params.eUsage & TextureUsage::Sampler) {
    VkSamplerCustomBorderColorCreateInfoEXT bci {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT
    };
    VkSamplerReductionModeCreateInfo rci {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
      .reductionMode = translateReduction(params.eFiltering)
    };
    VkSamplerCreateInfo sci {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = &rci,
      .magFilter = translate(params.eFilterMag),
      .minFilter = translate(params.eFilterMin),
      .mipmapMode = translateMipMode(params.eFilterMips),
      .addressModeU = translate(params.eWrapU),
      .addressModeV = translate(params.eWrapV),
      .addressModeW = translate(params.eWrapW),
      .anisotropyEnable = (params.fMaxAnisotropy > 1.0f) ? true : false,
      .maxAnisotropy = params.fMaxAnisotropy,
      .maxLod = params.fMaxLod
    };
    if (params.ivBorderColor == ColorRGBAi{0,0,0,0} )
      sci.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    else if (params.ivBorderColor == ColorRGBAi{0,0,0,255} )
      sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    else if (params.ivBorderColor == ColorRGBAi{255,255,255,255} )
      sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    else {
      sci.borderColor = VK_BORDER_COLOR_INT_CUSTOM_EXT;
      bci.customBorderColor.int32[0] = params.ivBorderColor[0];
      bci.customBorderColor.int32[1] = params.ivBorderColor[1];
      bci.customBorderColor.int32[2] = params.ivBorderColor[2];
      bci.customBorderColor.int32[3] = params.ivBorderColor[3];
      bci.format = VK_FORMAT_UNDEFINED;
      rci.pNext = &bci;
    }
    if (!VKCHECK(vkCreateSampler(context_.device_, &sci, nullptr, &sampler_)) || !sampler_) {
      debugPrint(DebugSeverity::Error, "Failed to create image sampler.");
    }
  }

  // Set the debug name.
  if (context_.gpu_.enabledFeatures & Feature::Validation && params.sDebugName) {
    VkDebugUtilsObjectNameInfoEXT info { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    info.objectType = VK_OBJECT_TYPE_IMAGE;
    info.objectHandle = (uint64_t)image_;
    std::string name = fmt::format("{}.image", params.sDebugName);
    info.pObjectName = name.c_str();
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(context_.device_, &info)))
      return;

    info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    info.objectHandle = (uint64_t)view_;
    name = fmt::format("{}.view", params.sDebugName);
    info.pObjectName = name.c_str();
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(context_.device_, &info)))
      return;

    if (sampler_) {
      info.objectType = VK_OBJECT_TYPE_SAMPLER;
      info.objectHandle = (uint64_t)sampler_;
      name = fmt::format("{}.sampler", params.sDebugName);
      info.pObjectName = name.c_str();
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(context_.device_, &info)))
        return;
    }
  }

  layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  accessMask_ = VK_ACCESS_NONE;
  stageMask_ = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

  initSuccess_ = true;
}

wcgl::Texture::~Texture() {
  if (sampler_) vkDestroySampler(context_.device_, sampler_, nullptr);
  if (view_) vkDestroyImageView(context_.device_, view_, nullptr);
  if (allocation_ && image_) vmaDestroyImage(context_.allocator_, image_, allocation_);
}

wcgl::Format wcgl::Texture::format() const {
  return translate(format_);
}

void wcgl::Texture::barrier(
  VkCommandBuffer cmd,
  VkImageLayout dstLayout,
  VkAccessFlags dstAccessMask,
  VkPipelineStageFlags dstStageMask)
{
  if (layout_ == dstLayout && accessMask_ == dstAccessMask && stageMask_ == dstStageMask)
    return;

  VkImageMemoryBarrier imgBarrier {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = accessMask_,
    .dstAccessMask = dstAccessMask,
    .oldLayout = layout_,
    .newLayout = dstLayout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image_,
    .subresourceRange = {
      .aspectMask = translateAspect(format_),
      .baseMipLevel = mipIndex_,
      .levelCount = mipCount_,
      .baseArrayLayer = 0,
      .layerCount = 1 }
  };
  vkCmdPipelineBarrier(cmd, stageMask_, dstStageMask, 0,
    0, nullptr, 0, nullptr, 1, &imgBarrier);

  layout_ = dstLayout;
  accessMask_ = dstAccessMask;
  stageMask_ = dstStageMask;
}