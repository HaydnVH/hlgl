#include "wcgl-vk-includes.h"
#include "wcgl-vk-debug.h"
#include "wcgl-vk-translate.h"
#include "wcgl/core/wcgl-context.h"
#include "wcgl/core/wcgl-buffer.h"


wcgl::Buffer::Buffer(const Context& context, BufferParams params)
: context_(context)
{
  VkBufferUsageFlags usage{0};

  if (params.usage & BufferUsage::TransferSrc)
    usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  if ((params.usage & BufferUsage::TransferDst) || params.pData)
    usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  if ((params.usage & BufferUsage::DeviceAddressable) && (context_.gpu_.enabledFeatures & Feature::BufferDeviceAddress))
    usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  if (params.usage & BufferUsage::Index)
    usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

  // TODO: Fill out more of the vk usage flags based on params usage flags.

  size_ = params.iSize;
  VkBufferCreateInfo bci{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = params.iSize,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
  };
  VmaAllocationCreateInfo aci{
    .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .usage = (params.usage & BufferUsage::HostMemory) ? VMA_MEMORY_USAGE_CPU_ONLY : VMA_MEMORY_USAGE_GPU_ONLY
  };
  if ((vmaCreateBuffer(context_.allocator_, &bci, &aci, &buffer_, &allocation_, &allocInfo_) != VK_SUCCESS) || !buffer_) {
    debugPrint(DebugSeverity::Error, "Failed to create buffer.");
    return;
  }

  // Copy provided data into the buffer.
  if (params.pData) {
    // If the buffer is on the CPU, we can just copy it over.
    if (params.usage & BufferUsage::HostMemory) {
      memcpy(allocInfo_.pMappedData, params.pData, params.iSize);
    }
    // If the buffer is on the GPU, we have to create a CPU-side staging buffer and then do a copy.
    else {
      // TODO: Optimize! The staging buffer could be re-used, and the transfer could be put on another thread or use a separate queue.
      Buffer stagingBuffer(context_, BufferParams{
        .usage = BufferUsage::TransferSrc | BufferUsage::HostMemory,
        .iSize = params.iSize,
        .pData = params.pData,});

      context_.immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy info{.srcOffset = 0, .dstOffset = 0, .size = params.iSize};
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer_, buffer_, 1, &info);
      });
    }
  }

  // Get the device address for our new buffer.
  if ((params.usage & BufferUsage::DeviceAddressable) && (context_.gpu_.enabledFeatures & Feature::BufferDeviceAddress)) {
    VkBufferDeviceAddressInfo info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = buffer_};
    deviceAddress_ = vkGetBufferDeviceAddress(context_.device_, &info);
  }

  indexSize_ = params.iIndexSize;
  accessMask_ = VK_ACCESS_NONE;
  stageMask_ = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

  initSuccess_ = true;
}

wcgl::Buffer::~Buffer() {
  if (allocation_ && buffer_) {
    vmaDestroyBuffer(context_.allocator_, buffer_, allocation_);
  }
}

void wcgl::Buffer::barrier(VkCommandBuffer cmd,
                               VkAccessFlags dstAccessMask,
                               VkPipelineStageFlags dstStageMask) {
  VkBufferMemoryBarrier bfrBarrier{
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .srcAccessMask = accessMask_,
    .dstAccessMask = dstAccessMask,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .buffer = buffer_,
    .size = size_};
  vkCmdPipelineBarrier(cmd, stageMask_, dstStageMask, 0,
                       0, nullptr, 1, &bfrBarrier, 0, nullptr);

  accessMask_ = dstAccessMask;
  stageMask_ = dstStageMask;
}