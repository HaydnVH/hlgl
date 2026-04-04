#include "vkimpl-includes.h"
#include "vkimpl-debug.h"
#include "vkimpl-translate.h"
#include "vkimpl-context.h"
#include <hlgl/context.h>
#include <hlgl/texture.h>
#include <hlgl/buffer.h>

void hlgl::Texture::Construct(TextureParams params)
{
  if (isValid()) {
    debugPrint(DebugSeverity::Error, "Attempting to Construct a texture that's already valid.");
    return;
  }

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
    params.filterMips = (params.mipCount > 1) ? params.filtering : FilterMode::Nearest;
 
  if (params.usage & TextureUsage::ScreenSize) {
    context::getDisplaySize(params.width, params.height);
    params.depth = 1;
  }

  if (params.width == 0 || params.height == 0 || params.depth == 0) {
    debugPrint(DebugSeverity::Error, "Texture must have non-zero dimensions.");
    return;
  }

  if (params.mipCount == 0) {
    debugPrint(DebugSeverity::Error, "Texture must have non-zero mip count.");
    return;
  }

  if (params.format == Format::Undefined) {
    debugPrint(DebugSeverity::Error, "Texture must have a defined format.");
    return;
  }

  // Figure out usage flags.
  VkImageUsageFlags usage {0};

  if (params.usage & TextureUsage::TransferSrc)
    usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  if ((params.usage & TextureUsage::TransferDst) || params.dataPtr)
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  if (params.usage & TextureUsage::Framebuffer) {
    if (params.dataPtr) {
      debugPrint(DebugSeverity::Error, "Can't create a framebuffer texture with existing data.");
      return;
    }

    if (translateAspect(format_) & VK_IMAGE_ASPECT_COLOR_BIT)
      usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    else if (translateAspect(format_) & VK_IMAGE_ASPECT_DEPTH_BIT) {
      usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      // Make sure the requested depth format is supported.
      // Either "D24S8" or "D32fS8" are guaranteed to be supported, as per Vulkan spec.
      if (!context::isDepthFormatSupported(params.format)) {
        switch (params.format) {
          case Format::D32f:
            if (context::isDepthFormatSupported(Format::D32fS8))
              params.format = Format::D32fS8;
            else
              params.format = Format::D24S8;
            break;
          case Format::D24S8:
            params.format = Format::D32fS8;
            break;
          case Format::D32fS8:
            params.format = Format::D24S8;
            break;
          default:
            break;
        }
      }
    }
  }

  if (params.usage & TextureUsage::Sampler)
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

  if (params.usage & TextureUsage::Storage)
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;

  extent_ = VkExtent3D{params.width, params.height, params.depth};
  mipIndex_ = params.mipBase;
  mipCount_ = params.mipCount;
  format_ = translate(params.format);

  layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  accessMask_ = VK_ACCESS_NONE;
  stageMask_ = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

  if (params._existingImage) {
    image_ = (VkImage)params._existingImage;
  }
  else {
    VkImageCreateInfo ici {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = (params.depth > 1) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
      .format = format_,
      .extent = extent_,
      .mipLevels = mipCount_,
      .arrayLayers = params.layerCount,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo aci{ .usage = VMA_MEMORY_USAGE_AUTO };
    if (params.usage & TextureUsage::Framebuffer)
      aci.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    if (!VKCHECK(vmaCreateImage(_impl::getAllocator(), &ici, &aci, &image_, &allocation_, &allocInfo_)) || !image_) {
      debugPrint(DebugSeverity::Error, "Failed to create image.");
      return;
    }
  }

  // Copy provided data into the texture.
  if (params.dataPtr) {
    // First we need to calculate the size of the image data, in bytes.
    size_t dataSize {params.width * params.height * params.depth * bytesPerPixel(params.format)};
    // TODO: Handle mipmaps and layers!

    // TODO: Optimize! The staging buffer could be re-used, and the transfer could be put on another thread or use a separate queue.
    Buffer stagingBuffer(BufferParams{
      .usage = BufferUsage::TransferSrc | BufferUsage::HostMemory,
      .data = {hlgl::DataPair{.size = dataSize, .ptr = params.dataPtr}},
      .debugName = "stagingBuffer"});

    // Copy data from the buffer to the image, transitioning layouts as needed.
    VkCommandBuffer cmd = _impl::beginImmediateCmd();
    barrier(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_MEMORY_WRITE_BIT, stageMask_);
    VkBufferImageCopy copy = {
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {
        .aspectMask = translateAspect(format_),
        .mipLevel = mipIndex_,
        .baseArrayLayer = params.layerBase,
        .layerCount = params.layerCount },
      .imageExtent = extent_ };

    vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer_[0], image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_MEMORY_READ_BIT, stageMask_);
    _impl::submitImmediateCmd(cmd);
  }

  VkImageViewCreateInfo vci {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = image_,
    .viewType = (params.depth > 1) ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D,
    .format = format_,
    .subresourceRange = {
      .aspectMask = translateAspect(format_),
      .baseMipLevel = mipIndex_,
      .levelCount = mipCount_,
      .baseArrayLayer = params.layerBase,
      .layerCount = params.layerCount } };
  if (!VKCHECK(vkCreateImageView(_impl::getDevice(), &vci, nullptr, &view_)) || !view_) {
    debugPrint(DebugSeverity::Error, "Failed to create image view.");
    if (allocation_) {
      vmaDestroyImage(_impl::getAllocator(), image_, allocation_);
      image_ = nullptr;
      allocation_ = nullptr;
    }
    return;
  }

  // Create the sampler.
  if (params.usage & TextureUsage::Sampler) {
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
    if (!VKCHECK(vkCreateSampler(_impl::getDevice(), &sci, nullptr, &sampler_)) || !sampler_) {
      debugPrint(DebugSeverity::Error, "Failed to create image sampler.");
      return;
    }
  }

  // Set the debug name.
  if (!params.debugName.empty() && _impl::isValidationEnabled()) {
    VkDebugUtilsObjectNameInfoEXT info { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    info.objectType = VK_OBJECT_TYPE_IMAGE;
    info.objectHandle = (uint64_t)image_;
    std::string name = std::format("{}.image", params.debugName);
    info.pObjectName = name.c_str();
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(_impl::getDevice(), &info)))
      debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", name));

    info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    info.objectHandle = (uint64_t)view_;
    name = std::format("{}.view", params.debugName);
    info.pObjectName = name.c_str();
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(_impl::getDevice(), &info)))
      debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", name));

    if (sampler_) {
      info.objectType = VK_OBJECT_TYPE_SAMPLER;
      info.objectHandle = (uint64_t)sampler_;
      name = std::format("{}.sampler", params.debugName);
      info.pObjectName = name.c_str();
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(_impl::getDevice(), &info)))
        debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", name));
    }
  }

  if (params.usage & TextureUsage::ScreenSize) {
    _impl::registerScreenSizeTexture(this);
  }

  savedParams_ = std::move(params);
  initSuccess_ = true;
}

hlgl::Texture::~Texture() {
  if (savedParams_.usage & TextureUsage::ScreenSize)
    _impl::unregisterScreenSizeTexture(this);

  _impl::queueDeletion(_impl::DelQueueTexture{.image = image_, .allocation = allocation_, .view = view_, .sampler = sampler_});
}

hlgl::Format hlgl::Texture::format() const { return translate(format_); }
uint32_t hlgl::Texture::getWidth() const { return extent_.width; }
uint32_t hlgl::Texture::getHeight() const { return extent_.height; }
uint32_t hlgl::Texture::getDepth() const { return extent_.depth; }

bool hlgl::Texture::resize() {
  if (savedParams_.dataPtr) {
    debugPrint(DebugSeverity::Error, "Can't resize a texture which was created with data.");
    return false;
  }

  _impl::queueDeletion(_impl::DelQueueTexture{.image = image_, .allocation = allocation_, .view = view_, .sampler = sampler_});
  initSuccess_ = false;
  Construct(savedParams_);
  return true;
}

bool hlgl::Texture::resize(uint32_t newWidth, uint32_t newHeight, uint32_t newDepth) {
  uint32_t oldWidth = savedParams_.width, oldHeight = savedParams_.height, oldDepth = savedParams_.depth;
  savedParams_.width = newWidth;
  savedParams_.height = newHeight;
  savedParams_.depth = newDepth;
  if (!resize()) {
    savedParams_.width = oldWidth; savedParams_.height = oldHeight; savedParams_.depth = oldDepth;
    return false;
  }
  else return true;
}

void hlgl::Texture::barrier(
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