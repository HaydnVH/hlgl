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
  Updateable        = 1 << 8, // The buffer can be updated with new data from the host.
  Vertex            = 1 << 9, // The buffer will contain vertices (not neccessary if using buffer device address).
};
using BufferUsages = Flags<BufferUsage>;
template <> struct FlagsTraits<BufferUsage> {
  static constexpr bool isFlags {true};
  static constexpr int32_t numBits {10};
};

struct DataPair {
  size_t size {0};
  const void* ptr {nullptr};
};

struct BufferParams {
  BufferUsages usage{BufferUsage::None};
  // The number of bytes in each element of the index buffer.
  std::vector<DataPair> data {};
  uint32_t indexSize{4};
  std::string debugName{};
};

class Buffer {
  friend class Frame;
  friend class Texture;

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

public:
  Buffer(Buffer&& other) noexcept;
  Buffer& operator=(Buffer&&) noexcept;

  Buffer() {}
  Buffer(BufferParams&& params) { Construct(params); }
  void Construct(BufferParams params);
  ~Buffer();

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

  hlgl::DeviceAddress getDeviceAddress() const;

  void updateData(void* pData, Frame* frame);

private:
  bool initSuccess_{false};
  BufferParams savedParams_ {};

#if defined HLGL_GRAPHICS_API_VULKAN

  std::array<VkBuffer, 2> buffer_{nullptr};
  std::array<VmaAllocation, 2> allocation_{nullptr};
  std::array<VmaAllocationInfo, 2> allocInfo_{};
  std::array<VkDeviceAddress, 2> deviceAddress_{0};
  std::array<VkAccessFlags, 2> accessMask_{0};
  std::array<VkPipelineStageFlags, 2> stageMask_{0};

  VkDeviceSize size_{0};
  uint32_t indexSize_{4};
  bool hostVisible_{false};
  bool fifSynced_{false};

  void barrier(
    VkCommandBuffer cmd,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags dstStageMask,
    uint32_t frame = 0);

#endif // defined HLGL_GRAPHICS_API_x
};

} // namespace hlgl