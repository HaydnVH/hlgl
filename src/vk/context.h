#ifndef HLGL_VK_CONTEXT_H
#define HLGL_VK_CONTEXT_H

#include <hlgl.h>
#include "vulkan-headers.h"
#include <variant>
#include "../utils/observer.h"

namespace hlgl {

VkDevice getDevice();
VmaAllocator getAllocator();

Frame* getCurrentFrame();

VkQueue getGraphicsQueue();
VkQueue getPresentQueue();
VkQueue getComputeQueue();
VkQueue getTransferQueue();

const std::array<VkDescriptorSetLayout,3>& getDescSetLayouts();

VkCommandBuffer beginImmediateCmd();
void submitImmediateCmd(VkCommandBuffer cmd);

struct DelQueueBuffer {VkBuffer buffer; VmaAllocation allocation;};
struct DelQueueTexture {VkImage image; VkImageView view; VkSampler sampler; VmaAllocation allocation;};
struct DelQueuePipeline {VkPipeline pipeline; VkPipelineLayout layout;};
using DelQueueItem = std::variant<DelQueueBuffer, DelQueueTexture, DelQueuePipeline>;

// Push an item to the queue so it can be deleted at a later frame, after it is no longer in use.
void queueDeletion(DelQueueItem item);
// Delete all the items which ought to be deleted on the current frame.
void flushDelQueue();
// Delete all items that have been queued for deletion, no matter which frame.
void flushAllDelQueues();

// Register an observer and callback to execute when the display is resized.  Parameters are the new width and height of the display.
void observeDisplayResize(Observer<uint32_t,uint32_t>* observer, std::function<void(uint32_t,uint32_t)> callback);


} // namespace hlgl
#endif // HLGL_VK_CONTEXT_H