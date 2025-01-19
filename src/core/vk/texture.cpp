#include "vk-includes.h"
#include "vk-debug.h"
#include "vk-translate.h"
#include <hlgl/core/context.h>
#include <hlgl/core/texture.h>

hlgl::Texture::Texture(Texture&& other) noexcept
: context_(other.context_),
  initSuccess_(other.initSuccess_),
  debugName_(other.debugName_),
  savedParams_(other.savedParams_),
  image_(other.image_),
  allocation_(other.allocation_),
  allocInfo_(other.allocInfo_),
  view_(other.view_),
  sampler_(other.sampler_),
  extent_(other.extent_),
  mipIndex_(other.mipIndex_),
  mipCount_(other.mipCount_),
  format_(other.format_),
  layout_(other.layout_),
  accessMask_(other.accessMask_),
  stageMask_(other.stageMask_)
{
  other.initSuccess_ = false;
  other.debugName_.clear();
  other.image_ = nullptr;
  other.allocation_ = nullptr;
  other.allocInfo_ = {};
  other.view_ = nullptr;
  other.sampler_ = nullptr;
}

hlgl::Texture& hlgl::Texture::operator = (Texture&& other) noexcept {
  std::destroy_at(this);
  std::construct_at(this, std::move(other));
  return *this;
}

void hlgl::Texture::Construct(TextureParams params)
{
  if (isValid()) {
    debugPrint(DebugSeverity::Error, "Attempting to Construct a texture that's already valid.");
    return;
  }

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
    std::tie(params.iWidth, params.iHeight) = context_.getDisplaySize();
    params.iDepth = 1;

    // Save this texture to the context so it can be recreated when the screen is resized.
    context_.screenSizeTextures_.push_back(this);
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

  layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  accessMask_ = VK_ACCESS_NONE;
  stageMask_ = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

  // Figure out usage flags.
  VkImageUsageFlags usage {0};

  if (params.usage & TextureUsage::TransferSrc)
    usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  if ((params.usage & TextureUsage::TransferDst) || params.pData)
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  if (params.usage & TextureUsage::Framebuffer) {
    if (params.pData) {
      debugPrint(DebugSeverity::Error, "Can't create a framebuffer texture with data.");
      return;
    }

    if (translateAspect(format_) & VK_IMAGE_ASPECT_COLOR_BIT)
      usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    else if (translateAspect(format_) & VK_IMAGE_ASPECT_DEPTH_BIT)
      usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }

  if (params.usage & TextureUsage::Sampler)
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

  if (params.usage & TextureUsage::Storage)
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;

  if (params.pExistingImage) {
    image_ = (VkImage)params.pExistingImage;
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
      .tiling = (params.usage & TextureUsage::HostMemory) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo aci{
      .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .usage = (params.usage & TextureUsage::HostMemory) ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY
    };
    if (!VKCHECK(vmaCreateImage(context_.allocator_, &ici, &aci, &image_, &allocation_, &allocInfo_)) || !image_) {
      debugPrint(DebugSeverity::Error, "Failed to create image.");
      return;
    }
  }

  // Copy provided data into the texture.
  if (params.pData) {
    // First we need to calculate the size of the image data, in bytes.
    size_t dataSize {params.iWidth * params.iHeight * params.iDepth * bytesPerPixel(params.eFormat)};
    // TODO: Handle mipmaps and layers!

    // If the buffer is on the CPU, we can just copy it over.
    if (params.usage & TextureUsage::HostMemory) {
      memcpy(allocInfo_.pMappedData, params.pData, dataSize);
    }
    // If the buffer is on the GPU, we have to create a CPU-side staging buffer and then do a copy.
    else {
      // TODO: Optimize! The staging texture could be re-used, and the transfer could be put on another thread or use a separate queue.
      // Use a Buffer (not a Texture) to transfer data.
      Buffer stagingBuffer(context_, BufferParams{
        .usage = BufferUsage::TransferSrc | BufferUsage::HostMemory,
        .iSize = dataSize,
        .pData = params.pData,
        .sDebugName = "stagingBuffer"});

      // Copy data from the buffer to the image, transitioning layouts as needed.
      context_.immediateSubmit([&](VkCommandBuffer cmd) {
        barrier(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_MEMORY_WRITE_BIT, stageMask_);
        VkBufferImageCopy copy = {
          .bufferOffset = 0,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource = {
            .aspectMask = translateAspect(format_),
            .mipLevel = mipIndex_,
            .baseArrayLayer = params.iLayerBase,
            .layerCount = params.iLayerCount },
          .imageExtent = extent_ };

        vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer_, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_MEMORY_READ_BIT, stageMask_);
      });
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
  if (params.usage & TextureUsage::Sampler) {
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
  if (params.sDebugName) {
    debugName_ = params.sDebugName;
    if (context_.gpu_.enabledFeatures & Feature::Validation) {
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
  }

  savedParams_ = params;
  initSuccess_ = true;
}

hlgl::Texture::~Texture() {

  // Remove the reference to this texture from the context.
  for (auto it {context_.screenSizeTextures_.begin()}; it != context_.screenSizeTextures_.end(); ++it) {
    if (*it == this) {
      context_.screenSizeTextures_.erase(it);
      break;
    }
  }

  context_.queueDeletion(Context::DelQueueTexture{.image = image_, .allocation = allocation_, .view = view_, .sampler = sampler_});
}

hlgl::Format hlgl::Texture::format() const { return translate(format_); }
uint32_t hlgl::Texture::getWidth() const { return extent_.width; }
uint32_t hlgl::Texture::getHeight() const { return extent_.height; }
uint32_t hlgl::Texture::getDepth() const { return extent_.depth; }

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