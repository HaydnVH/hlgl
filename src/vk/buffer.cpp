#include "buffer.h"
#include "context.h"
#include "debug.h"
#include "frame.h"


hlgl::Buffer::Buffer(Buffer::CreateParams params)
: _pimpl(std::make_unique<BufferImpl>(std::move(params)))
{ if (!_pimpl->buffer[0]) _pimpl.reset(); }

hlgl::BufferImpl::BufferImpl(Buffer::CreateParams&& params)
{
  size = params.size;
  bool hasData {false};
  for (const Buffer::CreateParams::Data& pair : params.data) {
    size += pair.size;
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
    fifSynced = true;

  indexSize = params.indexSize;

  // TODO: Fill out more of the vk usage flags based on params usage flags.

  VkBufferCreateInfo bci{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
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

  for (uint32_t i{0}; i < (fifSynced ? 2 : 1); ++i) {
    if ((vmaCreateBuffer(getAllocator(), &bci, &aci, &buffer[i], &allocation[i], &allocInfo[i]) != VK_SUCCESS) || !buffer[i]) {
      debugPrint(DebugSeverity::Error, "Failed to create buffer.");
      return;
    }

    VkMemoryPropertyFlags memFlags{0};
    vmaGetAllocationMemoryProperties(getAllocator(), allocation[i], &memFlags);
    hostVisible = (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    // Copy provided data into the buffer.
    if (hasData) {

      if (hostVisible) {
        // Allocation ended up in mappable memory and is already mapped, so we can write to it directly.
        size_t offset {0};
        for (const Buffer::CreateParams::Data& pair: params.data) {
          memcpy((void*)((uint8_t*)allocInfo[i].pMappedData + offset), pair.ptr, pair.size);
          offset += pair.size;
        }
        vmaFlushAllocation(getAllocator(), allocation[i], 0, VK_WHOLE_SIZE);
      } else {
        // Allocation ended up in non-mappable memory, so a transfer using a staging buffer is required.
        // TODO: Optimize! The staging buffer could be re-used, and the transfer could be put on another thread or use a separate queue.
        Buffer stagingBuffer(Buffer::CreateParams{
          .usage = BufferUsage::TransferSrc | BufferUsage::HostVisible,
          .data = params.data,
          .debugName = "stagingBuffer"});

        VkCommandBuffer cmd = beginImmediateCmd();
        VkBufferCopy info{.srcOffset = 0, .dstOffset = 0, .size = size};
        vkCmdCopyBuffer(cmd, stagingBuffer._pimpl->buffer[0], buffer[i], 1, &info);
        submitImmediateCmd(cmd);
      }
    }

    // Get the device address for our new buffer.
    if (params.usage & BufferUsage::DeviceAddressable) {
      VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer[i]};
      deviceAddress[i] = vkGetBufferDeviceAddress(getDevice(), &info);
    }

    // Set the debug name.
    if (!params.debugName && isValidationEnabled()) {
      VkDebugUtilsObjectNameInfoEXT info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
      info.objectType = VK_OBJECT_TYPE_BUFFER;
      info.objectHandle = (uint64_t)buffer[i];
      std::string debugName = std::format("{}[{}]", params.debugName, i);
      info.pObjectName = fifSynced ? debugName.c_str() : params.debugName;
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(getDevice(), &info))) {
        debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for {}", debugName));
      }
    }

    accessMask[i] = VK_ACCESS_NONE;
    stageMask[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }
}

hlgl::Buffer::~Buffer() {
  if (!_pimpl) return;
  for (uint32_t i{0}; i < (_pimpl->fifSynced ? 2 : 1); ++i) {
    if (_pimpl->buffer[i] || _pimpl->allocation[i])
      queueDeletion(DelQueueBuffer{.buffer = _pimpl->buffer[i], .allocation = _pimpl->allocation[i]});
  }
}

hlgl::DeviceAddress hlgl::Buffer::getAddress(Frame* frame) const {
  if (!_pimpl) return 0;
  return (frame && _pimpl->fifSynced) ? _pimpl->deviceAddress[frame->frameIndex] : _pimpl->deviceAddress[0];
}

hlgl::DeviceSize hlgl::Buffer::getSize() const {
  return _pimpl ? _pimpl->size : 0;
}

VkBuffer hlgl::BufferImpl::getBuffer(Frame* frame) {
  return (frame && fifSynced) ? buffer[frame->frameIndex] : buffer[0];
}

void hlgl::BufferImpl::barrier(VkCommandBuffer cmd,
                           VkAccessFlags dstAccessMask,
                           VkPipelineStageFlags dstStageMask,
                           uint32_t frame)
{
  if (accessMask[frame] == dstAccessMask && stageMask[frame] == dstStageMask)
    return;
  VkBufferMemoryBarrier bfrBarrier{
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .srcAccessMask = accessMask[frame],
    .dstAccessMask = dstAccessMask,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .buffer = buffer[frame],
    .size = size};
  vkCmdPipelineBarrier(cmd, stageMask[frame], dstStageMask, 0,
                       0, nullptr, 1, &bfrBarrier, 0, nullptr);

  accessMask[frame] = dstAccessMask;
  stageMask[frame] = dstStageMask;
}

void hlgl::BufferImpl::updateData(void* pData, Frame* frame) {
  if (!fifSynced) {
    debugPrint(DebugSeverity::Error, "Can't update a buffer which wasn't created with the 'Updateable' usage flag.");
    return;
  }
  uint32_t frameIndex = (frame) ? frame->frameIndex : 0;

  auto transferFunc = [&](VkCommandBuffer cmd) {
    
    //barrier(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, frameIndex);
    if (hostVisible) {
      // Allocation ended up in mappable memory and is already mapped, so we can write to it directly.
      memcpy(allocInfo[frameIndex].pMappedData, pData, size);
      //vmaFlushAllocation(context_.allocator_, allocation_[frameIndex], 0, VK_WHOLE_SIZE);
    }
    else if (size <= 65526) {
      vkCmdUpdateBuffer(cmd, buffer [frameIndex], 0, size, pData);
    }
    else {
      // Allocation ended up in non-mappable memory, so a transfer using a staging buffer is required.
      // TODO: Optimize! The staging buffer could be re-used, and the transfer could be put on another thread or use a separate queue.
      Buffer stagingBuffer(Buffer::CreateParams{
        .usage = BufferUsage::TransferSrc | BufferUsage::HostVisible,
        .data = {{.ptr = pData, .size = size}},
        .debugName = "stagingBuffer"});
      VkBufferCopy info{.srcOffset = 0, .dstOffset = 0, .size = size};
      vkCmdCopyBuffer(cmd, stagingBuffer._pimpl->buffer[0], buffer[frameIndex], 1, &info);
    }
    //barrier(cmd, VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, frameIndex);
  };

  if (frame) {
    transferFunc(frame->cmd);
  }
  else {
    VkCommandBuffer cmd = beginImmediateCmd();
    transferFunc(cmd);
    submitImmediateCmd(cmd);
  }
}