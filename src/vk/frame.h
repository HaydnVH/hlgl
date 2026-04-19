#ifndef HLGL_VK_FRAME_H
#define HLGL_VK_FRAME_H

#include <hlgl.h>
#include "vulkan-includes.h"

namespace hlgl {

class Frame {
  Frame(const Frame&) = delete;
  Frame(Frame&&) = delete;
  Frame& operator=(const Frame&) = delete;
  Frame& operator=(Frame&&) = delete;

public:
  Frame() noexcept = default;

  VkCommandBuffer cmd {nullptr};
  VkSemaphore acquireSemaphore {nullptr};
  VkSemaphore submitSemaphore {nullptr};
  VkFence fence {nullptr};
  Image* swapchainImage {nullptr};
  Pipeline* boundPipeline {nullptr};
  Buffer* boundIndexBuffer {nullptr};
  DeviceSize boundIndexBufferOffset {0};
  int64_t frameCounter {-1};
  uint32_t frameIndex {0};
  bool inDrawingPass {false};
};

} // namespace hlgl
#endif // HLGL_VK_FRAME_H