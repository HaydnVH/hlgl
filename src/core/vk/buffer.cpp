#include "vk-includes.h"
#include "vk-debug.h"
#include "vk-translate.h"
#include <hlgl/core/context.h>
#include <hlgl/core/buffer.h>
#include <hlgl/core/frame.h>
#include <fmt/format.h>


hlgl::Buffer::Buffer(Buffer&& other) noexcept
: context_(other.context_),
  initSuccess_(other.initSuccess_),
  debugName_(other.debugName_),
  buffer_(other.buffer_),
  allocation_(other.allocation_),
  allocInfo_(other.allocInfo_),
  deviceAddress_(other.deviceAddress_),
  accessMask_(other.accessMask_),
  stageMask_(other.stageMask_),
  size_(other.size_),
  indexSize_(other.indexSize_),
  hostVisible_(other.hostVisible_),
  fifSynced_(other.fifSynced_)
{
  other.initSuccess_ = false;
  other.debugName_.clear();
  other.buffer_ ={};
  other.allocation_ ={};
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

  if (params.usage & BufferUsage::Storage)
    usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  if (params.usage & BufferUsage::Uniform)
    usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  if (params.usage & BufferUsage::Updateable)
    fifSynced_ = true;

  indexSize_ = params.iIndexSize;

  // TODO: Fill out more of the vk usage flags based on params usage flags.

  size_ = params.iSize;
  VkBufferCreateInfo bci{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = params.iSize,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
  };
  VmaAllocationCreateInfo aci{
    .flags = (params.usage & BufferUsage::HostMemory) ? (VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) : 0u,
    .usage = (params.usage & BufferUsage::HostMemory) ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
  };
  if (params.usage & BufferUsage::Uniform) {
    aci.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if (!(params.usage & BufferUsage::HostMemory))
      aci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
  }

  for (uint32_t i{0}; i < (fifSynced_ ? 2 : 1); ++i) {
    if ((vmaCreateBuffer(context_.allocator_, &bci, &aci, &buffer_[i], &allocation_[i], &allocInfo_[i]) != VK_SUCCESS) || !buffer_[i]) {
      debugPrint(DebugSeverity::Error, "Failed to create buffer.");
      return;
    }

    VkMemoryPropertyFlags memFlags{0};
    vmaGetAllocationMemoryProperties(context_.allocator_, allocation_[i], &memFlags);
    hostVisible_ = (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    // Copy provided data into the buffer.
    if (params.pData) {

      if (hostVisible_) {
        // Allocation ended up in mappable memory and is already mapped, so we can write to it directly.
        memcpy(allocInfo_[i].pMappedData, params.pData, params.iSize);
        vmaFlushAllocation(context_.allocator_, allocation_[i], 0, VK_WHOLE_SIZE);
      } else {
        // Allocation ended up in non-mappable memory, so a transfer using a staging buffer is required.
        // TODO: Optimize! The staging buffer could be re-used, and the transfer could be put on another thread or use a separate queue.
        Buffer stagingBuffer(context_, BufferParams{
          .usage = BufferUsage::TransferSrc | BufferUsage::HostMemory,
          .iSize = params.iSize,
          .pData = params.pData,
          .sDebugName = "stagingBuffer"});

        context_.immediateSubmit([&](VkCommandBuffer cmd) {
          VkBufferCopy info{.srcOffset = 0, .dstOffset = 0, .size = params.iSize};
          vkCmdCopyBuffer(cmd, stagingBuffer.buffer_[0], buffer_[i], 1, &info);
        });
      }
    }

    // Get the device address for our new buffer.
    if ((params.usage & BufferUsage::DeviceAddressable) && (context_.gpu_.enabledFeatures & Feature::BufferDeviceAddress)) {
      VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer_[i]};
      deviceAddress_[i] = vkGetBufferDeviceAddress(context_.device_, &info);
    }

    // Set the debug name.
    if (params.sDebugName) {
      debugName_ = params.sDebugName;
      if (context_.gpu_.enabledFeatures & Feature::Validation) {
        VkDebugUtilsObjectNameInfoEXT info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        info.objectType = VK_OBJECT_TYPE_BUFFER;
        info.objectHandle = (uint64_t)buffer_[i];
        info.pObjectName = fifSynced_ ? fmt::format("{}[{}]", params.sDebugName, i).c_str() : params.sDebugName;
        if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(context_.device_, &info)))
          return;
      }
    }

    accessMask_[i] = VK_ACCESS_NONE;
    stageMask_[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }


  initSuccess_ = true;
}

hlgl::Buffer::~Buffer() {
  for (uint32_t i{0}; i < (fifSynced_ ? 2 : 1); ++i) {
    context_.queueDeletion(Context::DelQueueBuffer{.buffer = buffer_[i], .allocation = allocation_[i]});
  }
}

hlgl::DeviceAddress hlgl::Buffer::getDeviceAddress() const {
  DeviceAddress result{fifSynced_ ? deviceAddress_[context_.frameIndex_] : deviceAddress_[0]};
  if (result == 0) {
    debugPrint(hlgl::DebugSeverity::Error, "Requesting buffer device address, but address is null.  Did you forget to set a feature or usage flag?");
  }
  return result;
}

void hlgl::Buffer::barrier(VkCommandBuffer cmd,
                           VkAccessFlags dstAccessMask,
                           VkPipelineStageFlags dstStageMask,
                           uint32_t frame)
{
  if (accessMask_[frame] == dstAccessMask && stageMask_[frame] == dstStageMask)
    return;
  VkBufferMemoryBarrier bfrBarrier{
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .srcAccessMask = accessMask_[frame],
    .dstAccessMask = dstAccessMask,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .buffer = buffer_[frame],
    .size = size_};
  vkCmdPipelineBarrier(cmd, stageMask_[frame], dstStageMask, 0,
                       0, nullptr, 1, &bfrBarrier, 0, nullptr);

  accessMask_[frame] = dstAccessMask;
  stageMask_[frame] = dstStageMask;
}

void hlgl::Buffer::updateData(void* pData, Frame* frame) {
  if (!fifSynced_) {
    debugPrint(DebugSeverity::Error, "Can't update a buffer which wasn't created with the 'Updateable' usage flag.");
    return;
  }
  uint32_t frameIndex = (frame) ? frame->getFrameIndex() : 0;

  auto transferFunc = [&](VkCommandBuffer cmd) {
    
    //barrier(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, frameIndex);
    if (hostVisible_) {
      // Allocation ended up in mappable memory and is already mapped, so we can write to it directly.
      memcpy(allocInfo_[frameIndex].pMappedData, pData, size_);
      //vmaFlushAllocation(context_.allocator_, allocation_[frameIndex], 0, VK_WHOLE_SIZE);
    }
    else if (size_ <= 65526) {
      vkCmdUpdateBuffer(cmd, buffer_ [frameIndex], 0, size_, pData);
    }
    else {
      // Allocation ended up in non-mappable memory, so a transfer using a staging buffer is required.
      // TODO: Optimize! The staging buffer could be re-used, and the transfer could be put on another thread or use a separate queue.
      Buffer stagingBuffer(context_, BufferParams{
        .usage = BufferUsage::TransferSrc | BufferUsage::HostMemory,
        .iSize = size_,
        .pData = pData,
        .sDebugName = "stagingBuffer"});
      VkBufferCopy info{.srcOffset = 0, .dstOffset = 0, .size = size_};
      vkCmdCopyBuffer(cmd, stagingBuffer.buffer_[0], buffer_[frameIndex], 1, &info);
    }
    //barrier(cmd, VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, frameIndex);
  };

  if (frame) {
    VkCommandBuffer cmd = context_.getCommandBuffer();
    transferFunc(cmd);
  }
  else {
    context_.immediateSubmit([&](VkCommandBuffer cmd) {
      transferFunc(cmd);
    });
  }
}