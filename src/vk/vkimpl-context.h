#pragma once

#include "vkimpl-includes.h"
#include <hlgl/context.h>

#include <variant>

namespace hlgl {
namespace _impl {

  struct FrameInFlight {
    VkCommandBuffer cmd {nullptr};
    VkSemaphore imageAvailable {nullptr};
    VkSemaphore renderFinished {nullptr};
    VkFence fence {nullptr}; };

  VkDevice getDevice();
  VmaAllocator getAllocator();

  // Returns true if validation is available and enabled.
  bool isValidationEnabled();

  FrameInFlight& getCurrentFrameInFlight();
  uint32_t getFrameIndex();

  Texture& getCurrentSwapchainTexture();
  VkSwapchainKHR getSwapchain();
  uint32_t getSwapchainIndex();

  void advanceFrame();
  bool acquireNextImage();

  VkQueue getGraphicsQueue();
  VkQueue getPresentQueue();

  VkCommandBuffer beginImmediateCmd();
  void submitImmediateCmd(VkCommandBuffer cmd);

  struct DelQueueBuffer {VkBuffer buffer; VmaAllocation allocation;};
  struct DelQueueTexture {VkImage image; VmaAllocation allocation; VkImageView view; VkSampler sampler;};
  struct DelQueuePipeline {VkPipeline pipeline; VkPipelineLayout layout; VkDescriptorSetLayout descLayout;};
  using DelQueueItem = std::variant<DelQueueBuffer, DelQueueTexture, DelQueuePipeline>;

  // Push an item to the queue so it can be deleted at a later frame, after it is no longer in use.
  void queueDeletion(DelQueueItem item);
  // Delete all the items which ought to be deleted on the current frame.
  void flushDelQueue();
  // Delete all items that have been queued for deletion, no matter which frame.
  void flushAllDelQueues();

  // Check to see if the swapchain ought to be rebuilt, and rebuild it if so.
  bool resizeIfNeeded();

  void registerScreenSizeTexture(Texture* texture);
  void unregisterScreenSizeTexture(Texture* texture);

}} // namespace hlgl::_impl