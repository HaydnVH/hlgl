#include "vk-includes.h"
#include "vk-debug.h"
#include "vk-translate.h"
#include <hlgl/core/context.h>
#include <hlgl/core/buffer.h>
#include <fmt/format.h>


hlgl::Buffer::Buffer(Buffer&& other) noexcept
: context_(other.context_),
  initSuccess_(other.initSuccess_),
  debugName_(other.debugName_),
  buffer_(other.buffer_),
  allocation_(other.allocation_),
  allocInfo_(other.allocInfo_),
  size_(other.size_),
  deviceAddress_(other.deviceAddress_),
  indexSize_(other.indexSize_),
  accessMask_(other.accessMask_),
  stageMask_(other.stageMask_)
{
  other.initSuccess_ = false;
  other.debugName_.clear();
  other.buffer_ = nullptr;
  other.allocation_ = nullptr;
  other.allocInfo_ ={};
}

hlgl::Buffer& hlgl::Buffer::operator = (Buffer&& other) noexcept {
  std::destroy_at(this);
  std::construct_at(this, std::move(other));
  return *this;
}


void hlgl::Buffer::Construct(BufferParams params)
{
  if (isValid()) {
    debugPrint(DebugSeverity::Error, "Attempting to Construct a buffer that's already valid.");
    return;
  }

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
        .pData = params.pData,
        .sDebugName = "stagingBuffer"});

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

  // Set the debug name.
  if (params.sDebugName) {
    debugName_ = params.sDebugName;
    if (context_.gpu_.enabledFeatures & Feature::Validation) {
      VkDebugUtilsObjectNameInfoEXT info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
      info.objectType = VK_OBJECT_TYPE_BUFFER;
      info.objectHandle = (uint64_t)buffer_;
      info.pObjectName = params.sDebugName;
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(context_.device_, &info)))
        return;
    }
  }

  accessMask_ = VK_ACCESS_NONE;
  stageMask_ = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

  initSuccess_ = true;
}

hlgl::Buffer::~Buffer() {
  context_.queueDeletion(Context::DelQueueBuffer{.buffer = buffer_, .allocation = allocation_});
}

hlgl::DeviceAddress hlgl::Buffer::getDeviceAddress() const {
  if (deviceAddress_ == 0) {
    debugPrint(hlgl::DebugSeverity::Error, "Requesting buffer device address, but address is null.  Did you forget to set a feature or usage flag?");
  }
  return deviceAddress_;
}

void hlgl::Buffer::barrier(VkCommandBuffer cmd,
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