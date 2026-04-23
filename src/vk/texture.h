#ifndef HLGL_VK_TEXTURE_H
#define HLGL_VK_TEXTURE_H

#include <hlgl.h>
#include "vulkan-headers.h"
#include "../utils/observer.h"

namespace hlgl {

struct TextureImpl {
  TextureImpl(Texture::CreateParams&& params);

  std::string debugName;
  VkImage image{nullptr};
  VkImageView view{nullptr};
  VkSampler sampler {nullptr};
  VmaAllocation allocation{nullptr};
  VmaAllocationInfo allocInfo{};
  VkExtent3D extent{1,1,1};
  uint32_t mipBase{0};
  uint32_t mipCount{1};
  uint32_t layerBase{0};
  uint32_t layerCount{1};
  VkFormat format{VK_FORMAT_UNDEFINED};
  VkImageUsageFlags usage {0};
  VkImageCreateFlags flags {0};

  VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkAccessFlags accessMask{VK_ACCESS_NONE};
  VkPipelineStageFlags stageMask{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};

  uint32_t descIndexImageSampler {0};
  uint32_t descIndexStorageImage {0};

  Observer<uint32_t,uint32_t> displayResizeObserver {};

  void barrier(VkCommandBuffer cmd,
    VkImageLayout dstLayout,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags dstStageMask,
    uint32_t srcQfi = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQfi = VK_QUEUE_FAMILY_IGNORED);

  bool create(VkImage existingImage);
  bool resize(VkExtent3D newExtent);
};

} // namespace hlgl
#endif // HLGL_VK_TEXTURE_H