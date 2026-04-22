#include "texture.h"
#include "buffer.h"
#include "context.h"
#include "frame.h"
#include <chrono>

hlgl::Texture::Texture(Texture::CreateParams params)
: _pimpl(std::make_unique<TextureImpl>(std::move(params)))
{ if (!_pimpl->image || !_pimpl->view) _pimpl.reset(); }

hlgl::TextureImpl::TextureImpl(Texture::CreateParams&& params)
{
  auto timeStart = std::chrono::high_resolution_clock::now();
  extent = VkExtent3D{params.width, params.height, params.depth};
  mipBase = params.mipBase;
  mipCount = params.mipCount;
  layerBase = params.layerBase;
  layerCount = params.layerCount;
  format = translate(params.format);
  debugName = params.debugName;

  if (params.usage & TextureUsage::ScreenSize) {
    getDisplaySize(extent.width, extent.height);
    extent.depth = 1;
  }

  if (extent.width == 0 || extent.height == 0 || extent.depth == 0) {
    DEBUG_ERROR("Image must have non-zero dimensions.");
    return;
  }

  if (mipCount == 0) {
    DEBUG_ERROR("Image must have non-zero mip count.");
    return;
  }

  if (format == VK_FORMAT_UNDEFINED) {
    DEBUG_ERROR("Image must have a defined format.");
    return;
  }

  // Figure out usage flags.

  if (params.usage & TextureUsage::TransferSrc)
    usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  if ((params.usage & TextureUsage::TransferDst) || params.dataPtr)
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  if (params.sampler)
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

  if (params.usage & TextureUsage::Storage)
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;

  if (params.usage & TextureUsage::Framebuffer) {
    if (params.dataPtr) {
      DEBUG_ERROR("Can't create a framebuffer texture with existing data.");
      return;
    }

    if (isFormatDepth(params.format)) {
      usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      // Make sure the requested depth format is supported.
      // Either "D24S8" or "D32fS8" are guaranteed to be supported, as per Vulkan spec.
      if (!isDepthFormatSupported(params.format)) {
        ImageFormat requestedFormat {params.format};
        switch (params.format) {
          case ImageFormat::D32f:
            if (isDepthFormatSupported(ImageFormat::D32fS8))
              params.format = ImageFormat::D32fS8;
            else
              params.format = ImageFormat::D24S8;
            break;
          case ImageFormat::D24S8:
            params.format = ImageFormat::D32fS8;
            break;
          case ImageFormat::D32fS8:
            params.format = ImageFormat::D24S8;
            break;
          default:
            break;
        }
        DEBUG_WARNING(
          "Requested depth buffer format '%s' is unavailable on this device, using '%s' instead.",
          enumToStr(requestedFormat), enumToStr(params.format));
      }
    }
    else
      usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  // Create the image and view.
  if (!create((VkImage)params.extraData))
    return;

  // If the image is screen-sized, listen for when the screen is resized so it can remain so.
  if (params.usage & TextureUsage::ScreenSize) {
    observeDisplayResize(&displayResizeObserver, [this](uint32_t w,uint32_t h){
      resize({w, h, 1});
    });
  }

  // Copy provided data into the texture.
  if (params.dataPtr) {

    if (params.dataSize == 0 && !isFormatCompressed(params.format)) {
      // Calculate the size of the image data, in bytes.
      // TODO: Handle mipmaps!
      params.dataSize = params.width * params.height * params.depth * params.layerCount * bytesPerPixel(params.format);
    }

    transfer(this, 0, params.dataPtr, 0, params.dataSize, false);
  }

  // If the texture is flagged as a storage image, allocate and update a descriptor for it.
  if (params.usage & TextureUsage::Storage) {
    descIndexStorageImage = allocDescriptorIndex(DESC_TYPE_STORAGE_IMAGE);
    VkDescriptorImageInfo descInfo {
      .sampler = nullptr,
      .imageView = view,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
    VkWriteDescriptorSet descWrite {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = getDescriptorSet(DESC_TYPE_STORAGE_IMAGE),
      .dstArrayElement = descIndexStorageImage,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .pImageInfo = &descInfo };
    vkUpdateDescriptorSets(getDevice(), 1, &descWrite, 0, nullptr);
  }

  // Create the sampler for this texture.
  // TODO: Hash and cache the sampler parameters so multiple textures can share sampler objects.
  // TODO: Resizeable textures with mipmaps?
  if (params.sampler) {
    if (params.sampler->wrapU == WrapMode::DontCare)
      params.sampler->wrapU = params.sampler->wrapping;
    if (params.sampler->wrapV == WrapMode::DontCare)
      params.sampler->wrapV = params.sampler->wrapping;
    if (params.sampler->wrapW == WrapMode::DontCare)
      params.sampler->wrapW = params.sampler->wrapping;

    if (params.sampler->filterMin == FilterMode::DontCare)
      params.sampler->filterMin = params.sampler->filtering;
    if (params.sampler->filterMag == FilterMode::DontCare)
      params.sampler->filterMag = params.sampler->filtering;
    if (params.sampler->filterMips == FilterMode::DontCare)
      params.sampler->filterMips = (params.sampler->maxLod > 1.0f) ? params.sampler->filtering : FilterMode::Nearest;

    VkSamplerCustomBorderColorCreateInfoEXT bci {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT
    };
    VkSamplerReductionModeCreateInfo rci {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
      .reductionMode = translateReduction(params.sampler->filtering)
    };
    VkSamplerCreateInfo sci {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = &rci,
      .magFilter = translate(params.sampler->filterMag),
      .minFilter = translate(params.sampler->filterMin),
      .mipmapMode = translateMipMode(params.sampler->filterMips),
      .addressModeU = translate(params.sampler->wrapU),
      .addressModeV = translate(params.sampler->wrapV),
      .addressModeW = translate(params.sampler->wrapW),
      .anisotropyEnable = (params.sampler->maxAnisotropy > 1.0f) ? true : false,
      .maxAnisotropy = params.sampler->maxAnisotropy,
      .maxLod = params.sampler->maxLod
    };
    if (params.sampler->borderColor == ColorRGBAi{0,0,0,0} )
      sci.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    else if (params.sampler->borderColor == ColorRGBAi{0,0,0,255} )
      sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    else if (params.sampler->borderColor == ColorRGBAi{255,255,255,255} )
      sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    else {
      sci.borderColor = VK_BORDER_COLOR_INT_CUSTOM_EXT;
      bci.customBorderColor.int32[0] = params.sampler->borderColor[0];
      bci.customBorderColor.int32[1] = params.sampler->borderColor[1];
      bci.customBorderColor.int32[2] = params.sampler->borderColor[2];
      bci.customBorderColor.int32[3] = params.sampler->borderColor[3];
      bci.format = VK_FORMAT_UNDEFINED;
      rci.pNext = &bci;
    }
    if (!VKCHECK(vkCreateSampler(getDevice(), &sci, nullptr, &sampler)) || !sampler) {
      DEBUG_ERROR("Failed to create image sampler.");
      return;
    }

    // With a sampler created, we can assign a descriptor index and update the descriptor set.
    descIndexImageSampler = allocDescriptorIndex(DESC_TYPE_COMBINED_IMAGE_SAMPLER);
    VkDescriptorImageInfo descInfo {
      .sampler = sampler,
      .imageView = view,
      .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet descWrite {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = getDescriptorSet(DESC_TYPE_COMBINED_IMAGE_SAMPLER),
      .dstArrayElement = descIndexImageSampler,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &descInfo };
    vkUpdateDescriptorSets(getDevice(), 1, &descWrite, 0, nullptr);
    
    if (params.debugName && isValidationEnabled()) {
      char debugNameStr[256]; snprintf(debugNameStr, 256, "%s.sampler", params.debugName);
      VkDebugUtilsObjectNameInfoEXT info { 
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SAMPLER,
        .objectHandle = (uint64_t)sampler,
        .pObjectName = debugNameStr,
      };
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(getDevice(), &info)))
        DEBUG_WARNING("Failed to set Vulkan debug name for '%s'.", debugNameStr);
    }
  }

  if (params.sampler) {
    // Transition the new image into a state appropriate for reading as a sampled texture.
    VkCommandBuffer cmd = beginImmediateCmd();
    barrier(cmd, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
    submitImmediateCmd(cmd);
  }

  auto timeEnd = std::chrono::high_resolution_clock::now();
  auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
  DEBUG_OBJCREATION("Created texture '%s' (took %.2fms)", params.debugName ? params.debugName : "?", (double)timeElapsed.count() / 1000.0);
}

bool hlgl::TextureImpl::create(VkImage existingImage) {

  if (image || view || sampler || allocation) {
    queueDeletion(DelQueueTexture{
      .image = image,
      .view = view,
      .sampler = sampler,
      .allocation = allocation});
    image = nullptr;
    view = nullptr;
    sampler = nullptr;
    allocation = nullptr;
  }

  if (existingImage) {
    image = existingImage;
  }
  else {
    VkImageCreateInfo ici {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = (extent.depth > 1) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = extent,
      .mipLevels = mipCount,
      .arrayLayers = layerCount,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo aci{ .usage = VMA_MEMORY_USAGE_AUTO };
    if (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
      aci.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    if (!VKCHECK(vmaCreateImage(getAllocator(), &ici, &aci, &image, &allocation, &allocInfo)) || !image) {
      DEBUG_ERROR("Failed to create image.");
      return false;
    }
  }

  VkImageViewCreateInfo vci {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = image,
    .viewType = (extent.depth > 1) ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D,
    .format = format,
    .subresourceRange = {
      .aspectMask = translateAspect(format),
      .baseMipLevel = mipBase,
      .levelCount = mipCount,
      .baseArrayLayer = layerBase,
      .layerCount = layerCount } };
  if (!VKCHECK(vkCreateImageView(getDevice(), &vci, nullptr, &view)) || !view) {
    DEBUG_ERROR("Failed to create image view.");
    if (allocation) {
      vmaDestroyImage(getAllocator(), image, allocation);
      image = nullptr;
      allocation = nullptr;
    }
    return false;
  }

  // Set the debug name.
  if (!debugName.empty() && isValidationEnabled()) {
    char debugNameStr[256];
    snprintf(debugNameStr, 256, "%s.image", debugName.c_str());
    VkDebugUtilsObjectNameInfoEXT info { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    info.objectType = VK_OBJECT_TYPE_IMAGE;
    info.objectHandle = (uint64_t)image;
    info.pObjectName = debugNameStr;
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(getDevice(), &info)))
      DEBUG_WARNING("Failed to set Vulkan debug name for '%s'.", debugNameStr);

    snprintf(debugNameStr, 256, "%s.view", debugName.c_str());
    info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    info.objectHandle = (uint64_t)view;
    info.pObjectName = debugNameStr;
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(getDevice(), &info)))
      DEBUG_WARNING("Failed to set Vulkan debug name for '%s'.", debugNameStr);
  }

  return true;
}

hlgl::Texture::~Texture() {
  if (!_pimpl) return;
  if (_pimpl->image || _pimpl->view || _pimpl->sampler || _pimpl->allocation) {
    queueDeletion(DelQueueTexture{
      .image = _pimpl->image,
      .view = _pimpl->view,
      .sampler = _pimpl->sampler,
      .allocation = _pimpl->allocation });
  }
  if (_pimpl->descIndexImageSampler)
    queueDeletion(DelQueueDescriptor{.set = DESC_TYPE_COMBINED_IMAGE_SAMPLER, .index = _pimpl->descIndexImageSampler});
  if (_pimpl->descIndexStorageImage)
    queueDeletion(DelQueueDescriptor{.set = DESC_TYPE_STORAGE_IMAGE, .index = _pimpl->descIndexStorageImage});
}

void hlgl::Texture::getDimensions(uint32_t& w, uint32_t& h, uint32_t& d) const {
  if (!_pimpl) return;
  VkExtent3D extent = _pimpl->extent;
  w = extent.width;
  h = extent.height;
  d = extent.depth;
}

hlgl::ImageFormat hlgl::Texture::getFormat() const {
  return _pimpl ? translate(_pimpl->format) : ImageFormat::Undefined;
}

uint32_t hlgl::Texture::getSamplerIndex() const {
  return _pimpl ? _pimpl->descIndexImageSampler : 0;
}

uint32_t hlgl::Texture::getStorageIndex() const {
  return _pimpl ? _pimpl->descIndexStorageImage : 0;
}

bool hlgl::TextureImpl::resize(VkExtent3D newExtent) {
  VkExtent3D oldExtent {extent};
  extent.width = newExtent.width;
  extent.height = newExtent.height;
  extent.depth = newExtent.depth;
  if (!create(nullptr)) {
    extent = oldExtent;
    return false;
  }
  else return true;
}

void hlgl::TextureImpl::barrier(
  VkCommandBuffer cmd,
  VkImageLayout dstLayout,
  VkAccessFlags dstAccessMask,
  VkPipelineStageFlags dstStageMask,
  uint32_t srcQfi, uint32_t dstQfi)
{
  if (layout == dstLayout && accessMask == dstAccessMask && stageMask == dstStageMask)
    return;

  VkImageMemoryBarrier imgBarrier {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = accessMask,
    .dstAccessMask = dstAccessMask,
    .oldLayout = layout,
    .newLayout = dstLayout,
    .srcQueueFamilyIndex = srcQfi,
    .dstQueueFamilyIndex = dstQfi,
    .image = image,
    .subresourceRange = {
      .aspectMask = translateAspect(format),
      .baseMipLevel = mipBase,
      .levelCount = mipCount,
      .baseArrayLayer = 0,
      .layerCount = 1 }
  };
  vkCmdPipelineBarrier(cmd, stageMask, dstStageMask, 0,
    0, nullptr, 0, nullptr, 1, &imgBarrier);

  layout = dstLayout;
  accessMask = dstAccessMask;
  stageMask = dstStageMask;
}

void hlgl::Texture::barrier(ImageLayout layout, bool read) {
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'Texture::barrier' outside of a frame.");
    return;
  }

  if (!frame->boundPipeline) {
    DEBUG_ERROR("Can't call 'Texture::barrier' without a bound pipeline.");
    return;
  }

  _pimpl->barrier(frame->cmd,
    translate(layout),
    (read) ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_SHADER_WRITE_BIT,
    (frame->boundPipeline->isCompute()) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
}