#pragma once

#include <hlgl/hlgl-buffer.h>

namespace hlgl {

class Buffer {
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

public:
  Buffer(Buffer&& other) noexcept;
  Buffer& operator=(Buffer&&) noexcept;

  Buffer() {}
  Buffer(BufferParams&& params) { Construct(params); }
  void Construct(BufferParams params);
  ~Buffer() { Destruct(); }
  void Destruct();

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

  uint64_t getSize() const { return static_cast<uint64_t>(size_); }
  hlgl::DeviceAddress getDeviceAddress() const;

  void updateData(void* pData, Frame* frame);

  
  bool initSuccess_{false};
  BufferParams savedParams_ {};

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
};

}