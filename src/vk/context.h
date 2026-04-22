#ifndef HLGL_VK_CONTEXT_H
#define HLGL_VK_CONTEXT_H

#include <hlgl.h>
#include "vulkan-headers.h"
#include <variant>
#include "../utils/observer.h"

namespace hlgl {

constexpr uint32_t DESC_TYPE_SAMPLER                {0};
constexpr uint32_t DESC_TYPE_COMBINED_IMAGE_SAMPLER {1};
constexpr uint32_t DESC_TYPE_STORAGE_IMAGE          {2};
constexpr uint32_t NUM_DESCRIPTOR_SETS              {3};

constexpr uint32_t DESCRIPTOR_COUNTS[] {1000, 20000, 1000};

VkDevice getDevice();
VmaAllocator getAllocator();
const VkPhysicalDeviceProperties& getDeviceProperties();

Frame* getCurrentFrame();

VkQueue getGraphicsQueue();
VkQueue getPresentQueue();
VkQueue getComputeQueue();
VkQueue getTransferQueue();

const std::array<VkDescriptorSetLayout,3>& getDescSetLayouts();
VkDescriptorSet getDescriptorSet(uint32_t set);
uint32_t allocDescriptorIndex(uint32_t set);
VkPipelineLayout getPipelineLayout();

Texture* getDefaultTextureNull();
Texture* getDefaultTextureWhite();
Texture* getDefaultTextureGray();
Texture* getDefaultTextureBlack();

VkCommandBuffer beginImmediateCmd();
void submitImmediateCmd(VkCommandBuffer cmd);

struct DelQueueBuffer {VkBuffer buffer; VmaAllocation allocation;};
struct DelQueueTexture {VkImage image; VkImageView view; VkSampler sampler; VmaAllocation allocation;};
struct DelQueuePipeline {VkPipeline pipeline; VkPipelineLayout layout;};
struct DelQueueDescriptor {uint32_t set; uint32_t index;};
using DelQueueItem = std::variant<DelQueueBuffer, DelQueueTexture, DelQueuePipeline, DelQueueDescriptor>;

// Push an item to the queue so it can be deleted at a later frame, after it is no longer in use.
void queueDeletion(DelQueueItem item);
// Delete all the items which ought to be deleted on the current frame.
void flushDelQueue();
// Delete all items that have been queued for deletion, no matter which frame.
void flushAllDelQueues();

// Register an observer and callback to execute when the display is resized.  Parameters are the new width and height of the display.
void observeDisplayResize(Observer<uint32_t,uint32_t>* observer, std::function<void(uint32_t,uint32_t)> callback);

// Copies data from memory into the dedicated staging buffer so it can be forwarded onto another destination.
void transferToStagingBuffer(const void* srcMem, size_t srcOffset, size_t size);
void transfer(BufferImpl* dstBuffer, DeviceSize dstOffset, const void* srcMem, size_t srcOffset, size_t size, bool useTransferQueue);               // Copies data from memory into a buffer.
void transfer(BufferImpl* dstBuffer, DeviceSize dstOffset, BufferImpl* srcBuffer, DeviceSize srcOffset, DeviceSize size, bool useTransferQueue);    // Copies data from one buffer into another buffer.
void transfer(TextureImpl* dstTexture, DeviceSize dstOffset, const void* srcmem, DeviceSize srcOffset, DeviceSize size, bool useTransferQueue);     // Copies data from memory into an image.
void transfer(TextureImpl* dstTexture, DeviceSize dstOffset, BufferImpl* srcBuffer, DeviceSize srcOffset, DeviceSize size, bool useTransferQueue);  // Copies data from a buffer into an image.


} // namespace hlgl
#endif // HLGL_VK_CONTEXT_H