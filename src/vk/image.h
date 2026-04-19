#ifndef HLGL_VK_IMAGE_H
#define HLGL_VK_IMAGE_H

#include <hlgl.h>
#include "vulkan-includes.h"
#include "../utils/observer.h"

namespace hlgl {

class Image {
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;
public:
  Image(CreateImageParams&& params, VkImage existingImage = nullptr);
  ~Image();

  Image(Image&&) noexcept = default;
  Image& operator=(Image&&) noexcept = default;

  VkImage getImage() { return image; }
  VkImageView getView() { return view; }

  bool isValid() const { return (image && view); }
  VkFormat getFormat() const { return format; }
  VkExtent3D getExtent() const { return extent; }
  uint32_t getMipCount() const { return mipCount; }

  void barrier(VkCommandBuffer cmd,
    VkImageLayout dstLayout,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags dstStageMask);

private:
  CreateImageParams savedParams {};
  Observer<uint32_t,uint32_t> displayResizeObserver {};

  VkImage image{nullptr};
  VkImageView view{nullptr};
  VmaAllocation allocation{nullptr};
  VmaAllocationInfo allocInfo{};
  VkExtent3D extent{1,1,1};
  uint32_t mipBase{0};
  uint32_t mipCount{1};
  uint32_t layerBase{0};
  uint32_t layerCount{1};
  VkFormat format{VK_FORMAT_UNDEFINED};
  VkImageUsageFlags usage {0};
  std::string debugName;

  VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkAccessFlags accessMask{VK_ACCESS_NONE};
  VkPipelineStageFlags stageMask{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};

  bool create(VkImage existingImage);
  bool resize(VkExtent3D newExtent);
};

} // namespace hlgl
#endif // HLGL_VK_IMAGE_H