#include "buffer.h"
#include "context.h"
#include "frame.h"
#include <chrono>

hlgl::Buffer::Buffer(Buffer::CreateParams params)
: _pimpl(std::make_unique<BufferImpl>(std::move(params)))
{ if (!_pimpl->buffer[0]) _pimpl.reset(); }

hlgl::BufferImpl::BufferImpl(Buffer::CreateParams&& params)
{
  auto timeStart = std::chrono::high_resolution_clock::now();

  size = params.size;
  bool hasData {false};
  for (const Buffer::CreateParams::Data& pair : params.data) {
    size += pair.size;
    if (pair.ptr) hasData = true;
  }

  // Add padding for alignment.
  {
    DeviceSize multiple {getDeviceProperties().limits.minUniformBufferOffsetAlignment};
    DeviceSize remainder {size % multiple};
    actualSize = size + (remainder ? (multiple - remainder) : 0);
  }

  VkBufferUsageFlags usage{0};

  if (params.usage & BufferUsage::TransferSrc)
    usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  if ((params.usage & BufferUsage::TransferDst) || hasData)
    usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  if ((params.usage & BufferUsage::DeviceAddressable))
    usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  if (params.usage & BufferUsage::Index)
    usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  
  if (params.usage & BufferUsage::Indirect)
    usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

  if (params.usage & BufferUsage::Storage)
    usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  if (params.usage & BufferUsage::Uniform)
    usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  // TODO: Improve this?
  // Right now we're allocating two buffers, one for each frame in flight, so the data can be updated and synchronized properly.
  // This works, but it's inefficient.  It should be possible to instead allocate one buffer that's twice as large and use an offset.
  if (params.usage & BufferUsage::Updateable) {
    fifSynced = true;
  }

  indexSize = params.indexSize;

  // TODO: Fill out more of the vk usage flags based on params usage flags.

  VkBufferCreateInfo bci{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
  };
  VmaAllocationCreateInfo aci{
    .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
    .usage = VMA_MEMORY_USAGE_AUTO
  };

  if (params.usage & BufferUsage::HostVisible)
    aci.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
  else
    aci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;

  for (uint32_t i{0}; i < (fifSynced ? 2 : 1); ++i) {
    if ((vmaCreateBuffer(getAllocator(), &bci, &aci, &buffer[i], &allocation[i], &allocInfo[i]) != VK_SUCCESS) || !buffer[i]) {
      DEBUG_ERROR("Failed to create buffer.");
      return;
    }

    VkMemoryPropertyFlags memFlags{0};
    vmaGetAllocationMemoryProperties(getAllocator(), allocation[i], &memFlags);
    hostVisible = ((memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && allocInfo[i].pMappedData);

    // Copy provided data into the buffer.
    if (hasData) {
      DeviceSize offset {0};
      for (const Buffer::CreateParams::Data& pair : params.data) {
        transfer(this, offset, pair.ptr, 0, pair.size, false);
        offset += pair.size;
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
      char debugName[256]; snprintf(debugName, 256, "%s[%u]", params.debugName, i);
      VkDebugUtilsObjectNameInfoEXT info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
      info.objectType = VK_OBJECT_TYPE_BUFFER;
      info.objectHandle = (uint64_t)buffer[i];
      info.pObjectName = fifSynced ? debugName : params.debugName;
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(getDevice(), &info))) {
        DEBUG_WARNING("Failed to set Vulkan debug name for {}", debugName);
      }
    }

    accessMask[i] = VK_ACCESS_NONE;
    stageMask[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }

  auto timeEnd = std::chrono::high_resolution_clock::now();
  auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
  DEBUG_OBJCREATION("Created buffer '%s' (took %.2fms)", params.debugName ? params.debugName : "?", (double)timeElapsed.count() / 1000.0);
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
                           uint32_t frame,
                           uint32_t srcQfi, uint32_t dstQfi)
{
  if (accessMask[frame] == dstAccessMask && stageMask[frame] == dstStageMask)
    return;
  VkBufferMemoryBarrier bfrBarrier{
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .srcAccessMask = accessMask[frame],
    .dstAccessMask = dstAccessMask,
    .srcQueueFamilyIndex = srcQfi,
    .dstQueueFamilyIndex = dstQfi,
    .buffer = buffer[frame],
    .offset = 0,
    .size = size};
  vkCmdPipelineBarrier(cmd, stageMask[frame], dstStageMask, 0,
                       0, nullptr, 1, &bfrBarrier, 0, nullptr);

  accessMask[frame] = dstAccessMask;
  stageMask[frame] = dstStageMask;
}

void hlgl::Buffer::barrier(bool read) {
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'Buffer::barrier' outside of a frame.");
    return;
  }

  if (!frame->boundPipeline) {
    DEBUG_ERROR("Can't call 'Buffer::barrier' without a bound pipeline.");
    return;
  }

  _pimpl->barrier(frame->cmd,
    (read) ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_SHADER_WRITE_BIT,
    (frame->boundPipeline->isCompute()) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    frame->frameIndex);
}

void hlgl::Buffer::updateData(void* data, size_t size, DeviceSize offset) {
  Frame* frame = getCurrentFrame();
  if (!frame) {
    DEBUG_ERROR("Can't call 'Buffer::update' outside of a frame.");
    return;
  }

  if (!_pimpl->fifSynced) {
    DEBUG_ERROR("Can't update a buffer which wasn't createed with the 'Updateable' flag.");
    return;
  }

  if (_pimpl->hostVisible) {
    memcpy((uint8_t*)(_pimpl->allocInfo[frame->frameIndex].pMappedData) + offset, data, size);
  }
  else if (size <= 65536) {
    vkCmdUpdateBuffer(frame->cmd, _pimpl->buffer[frame->frameIndex], offset, size, data);
  }
  else {
    Buffer stagingBuffer(Buffer::CreateParams{
      .usage = BufferUsage::TransferSrc | BufferUsage::HostVisible,
      .data = {{.ptr = data, .size = size}},
      .debugName = "stagingBuffer" });
    VkBufferCopy info{.srcOffset = 0, .dstOffset = offset, .size = size};
    vkCmdCopyBuffer(frame->cmd, stagingBuffer._pimpl->buffer[0], _pimpl->buffer[frame->frameIndex], 1, &info);
  }
}
