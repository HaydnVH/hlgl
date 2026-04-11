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

    if (translateAspect(_vk.format) & VK_IMAGE_ASPECT_COLOR_BIT)
      usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    else if (translateAspect(_vk.format) & VK_IMAGE_ASPECT_DEPTH_BIT) {
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

  _vk.extent = VkExtent3D{params.width, params.height, params.depth};
  _vk.mipIndex = params.mipBase;
  _vk.mipCount = params.mipCount;
  _vk.format = translate(params.format);

  _vk.layout = VK_IMAGE_LAYOUT_UNDEFINED;
  _vk.accessMask = VK_ACCESS_NONE;
  _vk.stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

  if (params._existingImage) {
    _vk.image = (VkImage)params._existingImage;
  }
  else {
    VkImageCreateInfo ici {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = (params.depth > 1) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
      .format = _vk.format,
      .extent = _vk.extent,
      .mipLevels = _vk.mipCount,
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

    if (!VKCHECK(vmaCreateImage(_impl::getAllocator(), &ici, &aci, &_vk.image, &_vk.allocation, &_vk.allocInfo)) || !_vk.image) {
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
      .usage = BufferUsage::TransferSrc | BufferUsage::HostVisible,
      .data = {hlgl::DataPair{.size = dataSize, .ptr = params.dataPtr}},
      .debugName = "stagingBuffer"});

    // Copy data from the buffer to the image, transitioning layouts as needed.
    VkCommandBuffer cmd = _impl::beginImmediateCmd();
    _vk.barrier(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_MEMORY_WRITE_BIT, _vk.stageMask);
    VkBufferImageCopy copy = {
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {
        .aspectMask = translateAspect(_vk.format),
        .mipLevel = _vk.mipIndex,
        .baseArrayLayer = params.layerBase,
        .layerCount = params.layerCount },
      .imageExtent = _vk.extent };

    vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer_[0], _vk.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    _vk.barrier(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_MEMORY_READ_BIT, _vk.stageMask);
    _impl::submitImmediateCmd(cmd);
  }

  VkImageViewCreateInfo vci {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = _vk.image,
    .viewType = (params.depth > 1) ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D,
    .format = _vk.format,
    .subresourceRange = {
      .aspectMask = translateAspect(_vk.format),
      .baseMipLevel = _vk.mipIndex,
      .levelCount = _vk.mipCount,
      .baseArrayLayer = params.layerBase,
      .layerCount = params.layerCount } };
  if (!VKCHECK(vkCreateImageView(_impl::getDevice(), &vci, nullptr, &_vk.view)) || !_vk.view) {
    debugPrint(DebugSeverity::Error, "Failed to create image view.");
    if (_vk.allocation) {
      vmaDestroyImage(_impl::getAllocator(), _vk.image, _vk.allocation);
      _vk.image = nullptr;
      _vk.allocation = nullptr;
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
    if (!VKCHECK(vkCreateSampler(_impl::getDevice(), &sci, nullptr, &_vk.sampler)) || !_vk.sampler) {
      debugPrint(DebugSeverity::Error, "Failed to create image sampler.");
      return;
    }
  }

  // Set the debug name.
  if (!params.debugName.empty() && _impl::isValidationEnabled()) {
    VkDebugUtilsObjectNameInfoEXT info { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    info.objectType = VK_OBJECT_TYPE_IMAGE;
    info.objectHandle = (uint64_t)_vk.image;
    std::string name = std::format("{}.image", params.debugName);
    info.pObjectName = name.c_str();
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(_impl::getDevice(), &info)))
      debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", name));

    info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    info.objectHandle = (uint64_t)_vk.view;
    name = std::format("{}.view", params.debugName);
    info.pObjectName = name.c_str();
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(_impl::getDevice(), &info)))
      debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", name));

    if (_vk.sampler) {
      info.objectType = VK_OBJECT_TYPE_SAMPLER;
      info.objectHandle = (uint64_t)_vk.sampler;
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

  _impl::queueDeletion(_impl::DelQueueTexture{
    .image = _vk.image,
    .allocation = _vk.allocation,
    .view = _vk.view, .sampler = _vk.sampler});
}

hlgl::Format hlgl::Texture::getFormat() const { return translate(_vk.format); }
uint32_t hlgl::Texture::getWidth() const { return _vk.extent.width; }
uint32_t hlgl::Texture::getHeight() const { return _vk.extent.height; }
uint32_t hlgl::Texture::getDepth() const { return _vk.extent.depth; }

bool hlgl::Texture::resize() {
  if (savedParams_.dataPtr) {
    debugPrint(DebugSeverity::Error, "Can't resize a texture which was created with data.");
    return false;
  }

  _impl::queueDeletion(_impl::DelQueueTexture{
    .image = _vk.image,
    .allocation = _vk.allocation,
    .view = _vk.view, .sampler = _vk.sampler});
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

void hlgl::Texture::VK::barrier(
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
      .baseMipLevel = mipIndex,
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