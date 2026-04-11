#include "vkimpl-includes.h"
#include "vkimpl-translate.h"
#include "vkimpl-context.h"
#include "vkimpl-debug.h"
#include <hlgl/context.h>
#include <hlgl/buffer.h>
#include <hlgl/frame.h>

hlgl::Buffer::Buffer(Buffer&& other) noexcept
: initSuccess_(other.initSuccess_),
  savedParams_(std::move(other.savedParams_)),
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

  size_ = 0;
  bool hasData {false};
  for (const DataPair& pair : params.data) {
    size_ += pair.size;
    if (pair.ptr) hasData = true;
  }

  VkBufferUsageFlags usage{0};

  if (params.usage & BufferUsage::DescriptorHeap) {
    params.usage |= BufferUsage::DeviceAddressable | BufferUsage::HostVisible;
    usage |= VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT;
  }

  if (params.usage & BufferUsage::TransferSrc)
    usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  if ((params.usage & BufferUsage::TransferDst) || hasData)
    usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  if ((params.usage & BufferUsage::DeviceAddressable))
    usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  if (params.usage & BufferUsage::Index)
    usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

  if (params.usage & BufferUsage::Storage)
    usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  if (params.usage & BufferUsage::Uniform)
    usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  // TODO: Improve this?
  // Right now we're allocating two buffers, one for each frame in flight, so the data can be updated and synchronized properly.
  // This works, but it's inefficient.  It should be possible to instead allocate one buffer that's twice as large and use an offset.
  if (params.usage & BufferUsage::Updateable)
    fifSynced_ = true;

  indexSize_ = params.indexSize;

  // TODO: Fill out more of the vk usage flags based on params usage flags.

  VkBufferCreateInfo bci{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size_,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
  };
  VmaAllocationCreateInfo aci{
    .flags = (params.usage & BufferUsage::HostVisible) ? (VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) : 0u,
    .usage = VMA_MEMORY_USAGE_AUTO
  };
  if (params.usage & BufferUsage::Uniform) {
    aci.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if (!(params.usage & BufferUsage::HostVisible))
      aci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
  }

  for (uint32_t i{0}; i < (fifSynced_ ? 2 : 1); ++i) {
    if ((vmaCreateBuffer(_impl::getAllocator(), &bci, &aci, &buffer_[i], &allocation_[i], &allocInfo_[i]) != VK_SUCCESS) || !buffer_[i]) {
      debugPrint(DebugSeverity::Error, "Failed to create buffer.");
      return;
    }

    VkMemoryPropertyFlags memFlags{0};
    vmaGetAllocationMemoryProperties(_impl::getAllocator(), allocation_[i], &memFlags);
    hostVisible_ = (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    // Copy provided data into the buffer.
    if (hasData) {

      if (hostVisible_) {
        // Allocation ended up in mappable memory and is already mapped, so we can write to it directly.
        size_t offset {0};
        for (const DataPair& pair: params.data) {
          memcpy((void*)((char*)allocInfo_[i].pMappedData + offset), pair.ptr, pair.size);
          offset += pair.size;
        }
        vmaFlushAllocation(_impl::getAllocator(), allocation_[i], 0, VK_WHOLE_SIZE);
      } else {
        // Allocation ended up in non-mappable memory, so a transfer using a staging buffer is required.
        // TODO: Optimize! The staging buffer could be re-used, and the transfer could be put on another thread or use a separate queue.
        Buffer stagingBuffer(BufferParams{
          .usage = BufferUsage::TransferSrc | BufferUsage::HostVisible,
          .data = params.data,
          .debugName = "stagingBuffer"});

        VkCommandBuffer cmd = _impl::beginImmediateCmd();
        VkBufferCopy info{.srcOffset = 0, .dstOffset = 0, .size = size_};
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer_[0], buffer_[i], 1, &info);
        _impl::submitImmediateCmd(cmd);
      }
    }

    // Get the device address for our new buffer.
    if (params.usage & BufferUsage::DeviceAddressable) {
      VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer_[i]};
      deviceAddress_[i] = vkGetBufferDeviceAddress(_impl::getDevice(), &info);
    }

    // Set the debug name.
    if (!params.debugName.empty() && _impl::isValidationEnabled()) {
      VkDebugUtilsObjectNameInfoEXT info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
      info.objectType = VK_OBJECT_TYPE_BUFFER;
      info.objectHandle = (uint64_t)buffer_[i];
      std::string debugName = std::format("{}[{}]", params.debugName, i);
      info.pObjectName = fifSynced_ ? debugName.c_str() : params.debugName.c_str();
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(_impl::getDevice(), &info))) {
        debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for {}", debugName));
      }
    }

    accessMask_[i] = VK_ACCESS_NONE;
    stageMask_[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }


  initSuccess_ = true;
}

void hlgl::Buffer::Destruct() {
  for (uint32_t i{0}; i < (fifSynced_ ? 2 : 1); ++i) {
    _impl::queueDeletion(_impl::DelQueueBuffer{.buffer = buffer_[i], .allocation = allocation_[i]});
    buffer_[i] = nullptr;
    allocation_[i] = nullptr;
    allocInfo_[i] = {};
    deviceAddress_[i] = 0;
    accessMask_[i] = 0;
    stageMask_[i] = 0;
  }
  size_ = 0;
  indexSize_ = 4;
  hostVisible_ = false;
  fifSynced_ = false;
}

hlgl::DeviceAddress hlgl::Buffer::getDeviceAddress() const {
  DeviceAddress result{fifSynced_ ? deviceAddress_[hlgl::_impl::getCurrentFrameIndex()] : deviceAddress_[0]};
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
  uint32_t frameIndex = (frame) ? _impl::getCurrentFrameIndex() : 0;

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
      Buffer stagingBuffer(BufferParams{
        .usage = BufferUsage::TransferSrc | BufferUsage::HostVisible,
        .data = {{.size = size_, .ptr = pData}},
        .debugName = "stagingBuffer"});
      VkBufferCopy info{.srcOffset = 0, .dstOffset = 0, .size = size_};
      vkCmdCopyBuffer(cmd, stagingBuffer.buffer_[0], buffer_[frameIndex], 1, &info);
    }
    //barrier(cmd, VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, frameIndex);
  };

  if (frame) {
    VkCommandBuffer cmd = _impl::getCurrentFrameCmdBuffer();
    transferFunc(cmd);
  }
  else {
    VkCommandBuffer cmd = _impl::beginImmediateCmd();
    transferFunc(cmd);
    _impl::submitImmediateCmd(cmd);
  }
}