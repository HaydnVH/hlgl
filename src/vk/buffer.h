#ifndef HLGL_VK_BUFFER_H
#define HLGL_VK_BUFFER_H

#include <hlgl.h>
#include "vulkan-headers.h"

namespace hlgl {

struct BufferImpl {
  BufferImpl(Buffer::CreateParams&& params);

  std::array<VkBuffer, 2> buffer{nullptr};
  std::array<VmaAllocation, 2> allocation{nullptr};
  std::array<VmaAllocationInfo, 2> allocInfo{};
  std::array<VkDeviceAddress, 2> deviceAddress{0};
  std::array<VkAccessFlags, 2> accessMask{0};
  std::array<VkPipelineStageFlags, 2> stageMask{0};

  VkDeviceSize size{0};
  VkDeviceSize actualSize{0};
  VkDeviceSize syncOffset{0};
  uint32_t indexSize{4};
  bool hostVisible{false};
  bool fifSynced{false};

  VkBuffer getBuffer(Frame* frame);
  VkDeviceAddress getDeviceAddress(Frame* frame) const;
  
  void barrier(
    VkCommandBuffer cmd,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags dstStageMask,
    uint32_t frame,
    uint32_t srcQfi = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQfi = VK_QUEUE_FAMILY_IGNORED);
};

} // namespace hlgl
#endif // HLGL_VK_BUFFER_H