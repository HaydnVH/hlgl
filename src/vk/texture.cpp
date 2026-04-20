#include "texture.h"
#include "buffer.h"
#include "context.h"
#include "debug.h"
#include "vulkan-translate.h"

hlgl::Texture::Texture(Texture::CreateParams params)
: _pimpl(std::make_unique<TextureImpl>(std::move(params)))
{ if (!_pimpl->image || !_pimpl->view) _pimpl.reset(); }

hlgl::TextureImpl::TextureImpl(Texture::CreateParams&& params)
{
  extent = VkExtent3D(params.width, params.height, params.depth);
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
    debugPrint(DebugSeverity::Error, "Image must have non-zero dimensions.");
    return;
  }

  if (mipCount == 0) {
    debugPrint(DebugSeverity::Error, "Image must have non-zero mip count.");
    return;
  }

  if (format == VK_FORMAT_UNDEFINED) {
    debugPrint(DebugSeverity::Error, "Image must have a defined format.");
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
      debugPrint(DebugSeverity::Error, "Can't create a framebuffer texture with existing data.");
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
        debugPrint(DebugSeverity::Warning, std::format(
          "Requested depth buffer format '{}' is unavailable on this device, using '{}' instead.",
          enumToStr(requestedFormat), enumToStr(params.format)));
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

    // TODO: Optimize! The staging buffer could be re-used, and the transfer could be put on another thread or use a separate queue.
    Buffer stagingBuffer(Buffer::CreateParams{
      .usage = BufferUsage::TransferSrc | BufferUsage::HostVisible,
      .data = {{.ptr = params.dataPtr, .size = params.dataSize}},
      .debugName = "stagingBuffer"});

    // Copy data from the buffer to the image, transitioning layouts as needed.
    VkCommandBuffer cmd = beginImmediateCmd();
    barrier(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_MEMORY_WRITE_BIT, stageMask);
    VkBufferImageCopy copy = {
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {
        .aspectMask = translateAspect(format),
        .mipLevel = mipBase,
        .baseArrayLayer = params.layerBase,
        .layerCount = params.layerCount },
      .imageExtent = extent };

    vkCmdCopyBufferToImage(cmd, stagingBuffer._pimpl->getBuffer(nullptr), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_MEMORY_READ_BIT, stageMask);
    submitImmediateCmd(cmd);
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
      debugPrint(DebugSeverity::Error, "Failed to create image sampler.");
      return;
    }

    if (params.debugName && isValidationEnabled()) {
      std::string name = std::format("{}.sampler", params.debugName);
      VkDebugUtilsObjectNameInfoEXT info { 
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SAMPLER,
        .objectHandle = (uint64_t)sampler,
        .pObjectName = name.c_str(),
      };
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(getDevice(), &info)))
        debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", name));
    }
  }
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
      debugPrint(DebugSeverity::Error, "Failed to create image.");
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
    debugPrint(DebugSeverity::Error, "Failed to create image view.");
    if (allocation) {
      vmaDestroyImage(getAllocator(), image, allocation);
      image = nullptr;
      allocation = nullptr;
    }
    return false;
  }

  // Set the debug name.
  if (!debugName.empty() && isValidationEnabled()) {
    VkDebugUtilsObjectNameInfoEXT info { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    info.objectType = VK_OBJECT_TYPE_IMAGE;
    info.objectHandle = (uint64_t)image;
    std::string name = std::format("{}.image", debugName);
    info.pObjectName = name.c_str();
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(getDevice(), &info)))
      debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", name));

    info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    info.objectHandle = (uint64_t)view;
    name = std::format("{}.view", debugName);
    info.pObjectName = name.c_str();
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(getDevice(), &info)))
      debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", name));
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
  VkPipelineStageFlags dstStageMask)
{
  if (layout == dstLayout && accessMask == dstAccessMask && stageMask == dstStageMask)
    return;

  VkImageMemoryBarrier imgBarrier {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = accessMask,
    .dstAccessMask = dstAccessMask,
    .oldLayout = layout,
    .newLayout = dstLayout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
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