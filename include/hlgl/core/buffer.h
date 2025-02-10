#pragma once

#include "types.h"

namespace hlgl {

class Context;

// How should this buffer be used?
enum class BufferUsage: uint32_t {
  None              = 0,
  DeviceAddressable = 1 << 1, // The buffer's device address can be retrieved and used.
  HostMemory        = 1 << 2, // The buffer will exist on host memory (system ram) instead of on the GPU's VRAM.
  Index             = 1 << 3, // The buffer will contain indices.
  Storage           = 1 << 4, // The buffer will be used for arbitrary data storage.
  TransferSrc       = 1 << 5, // The buffer will be used as the source for transfer operations.
  TransferDst       = 1 << 6, // The buffer will be used as the destination for transfer operations.
  Uniform           = 1 << 7, // The buffer will be used as a uniform buffer object.
  UploadNever       = 1 << 8, // The buffer should exist purely in GPU Vram and will never have data uploaded to it from the CPU.
  UploadOnce        = 1 << 9, // The buffer will have data transferred to it from the CPU during construction and then never again.
  UploadRarely      = 1 << 10,// The buffer will have data transferred to it from the CPU multiple times throughout its lifetime, but no more than once per frame and not on every frame.
  UploadEachFrame   = 1 << 11,// The buffer will have data transferred to it from the CPU once per frame on every frame.
  UploadFrequently  = 1 << 12,// The buffer will have data transferred to it from the CPU more than once per frame.
  Vertex            = 1 << 13, // The buffer will contain vertices (not neccessary if using buffer device address).
};
template <> struct isBitfield<BufferUsage>: public std::true_type {};
using BufferUsages = Flags<BufferUsage>;

struct BufferParams {
  BufferUsages usage{BufferUsage::None};
  // The number of bytes in each element of the index buffer.
  uint32_t iIndexSize{4};
  size_t iSize{0};
  const void* pData{0};
  const char* sDebugName{nullptr};
};

class Buffer {
  friend class Frame;
  friend class Texture;

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

public:
  Buffer(Buffer&& other) noexcept;
  Buffer& operator=(Buffer&&) noexcept;

  Buffer(Context& context): context_(context) {}
  Buffer(Context& context, BufferParams&& params): context_(context) { Construct(params); }
  void Construct(BufferParams params);
  ~Buffer();

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

  hlgl::DeviceAddress getDeviceAddress() const;

  void uploadData(void* pData, Frame* frame);

private:
  Context& context_;
  bool initSuccess_{false};
  std::string debugName_ {};

#if defined HLGL_GRAPHICS_API_VULKAN

  VkBuffer buffer_{nullptr};
  VmaAllocation allocation_{nullptr};
  VmaAllocationInfo allocInfo_{};
  VkDeviceSize size_{0};
  VkDeviceAddress deviceAddress_{0};
  uint32_t indexSize_{4};
  bool hostVisible_ {false};

  VkAccessFlags accessMask_{0};
  VkPipelineStageFlags stageMask_{0};

  void barrier(
    VkCommandBuffer cmd,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags dstStageMask);

#endif // defined HLGL_GRAPHICS_API_x
};

} // namespace hlgl