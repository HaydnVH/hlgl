#include "frame.h"
#include "context.h"
#include "buffer.h"
#include "pipeline.h"
#include "texture.h"

void hlgl::blitImage(Texture* dst, Texture* src, BlitRegion dstRegion, BlitRegion srcRegion, bool filterLinear) {
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'blitImage' outside of a frame.");
    return;
  }

  // If we started a draw pass, end it here.
  endDrawing();

  // Barrier transition the blit source.
  src->_pimpl->barrier(frame->cmd,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    VK_ACCESS_TRANSFER_READ_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Barrier transition the blit destination.
  dst->_pimpl->barrier(frame->cmd,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_ACCESS_TRANSFER_WRITE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT);

  if (dstRegion.screenRegion) {
    dstRegion.x = 0; dstRegion.y = 0; dstRegion.z = 0; dstRegion.d = 1;
    getDisplaySize(dstRegion.w, dstRegion.h);
  }

  if (srcRegion.screenRegion) {
    srcRegion.x = 0; srcRegion.y = 0; srcRegion.z = 0;  srcRegion.d = 1;
    getDisplaySize(srcRegion.w, srcRegion.h);
  }

  VkImageBlit2 region {
    .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
    .srcSubresource = {
      .aspectMask = translateAspect(src->getFormat()),
      .mipLevel = srcRegion.mipLevel,
      .baseArrayLayer = srcRegion.baseLayer,
      .layerCount = srcRegion.layerCount },
      .srcOffsets = {
        VkOffset3D{
        .x = (int32_t)srcRegion.x,
        .y = (int32_t)srcRegion.y,
        .z = (int32_t)srcRegion.z },
        VkOffset3D{
        .x = std::min<int32_t>(src->_pimpl->extent.width, srcRegion.x + srcRegion.w),
        .y = std::min<int32_t>(src->_pimpl->extent.height, srcRegion.y + srcRegion.h),
        .z = std::min<int32_t>(src->_pimpl->extent.depth, srcRegion.z + srcRegion.d)} },
      .dstSubresource = {
      .aspectMask = translateAspect(dst->getFormat()),
      .mipLevel = dstRegion.mipLevel,
      .baseArrayLayer = dstRegion.baseLayer,
      .layerCount = dstRegion.layerCount },
      .dstOffsets = {
        VkOffset3D{
        .x = (int32_t)dstRegion.x,
        .y = (int32_t)dstRegion.y,
        .z = (int32_t)dstRegion.z },
        VkOffset3D{
        .x = std::min<int32_t>(dst->_pimpl->extent.width, dstRegion.x + dstRegion.w),
        .y = std::min<int32_t>(dst->_pimpl->extent.height, dstRegion.y + dstRegion.h),
        .z = std::min<int32_t>(dst->_pimpl->extent.depth, dstRegion.z + dstRegion.d)} }
  };

  VkBlitImageInfo2 info {
    .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
    .srcImage = src->_pimpl->image,
    .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    .dstImage = dst->_pimpl->image,
    .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .regionCount = 1,
    .pRegions = &region,
    .filter = filterLinear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
  };
  vkCmdBlitImage2(frame->cmd, &info);
}

void hlgl::beginDrawing(std::initializer_list<ColorAttachment> colorAttachments, std::optional<DepthAttachment> depthAttachment) {
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'beginDrawing' outside of a frame.");
    return;
  }

  if (colorAttachments.size() <= 0) {
    DEBUG_ERROR("beginDrawing requires at least one color attachment to output to.");
    return;
  }

  // If we started a draw pass, end it before starting a new one.
  endDrawing();
  
  // Record the minimum extent of each attachment so we don't accidentally try to draw outside the framebuffers.
  VkExtent2D viewportExtent {};
  getDisplaySize(viewportExtent.width, viewportExtent.height);

  // Transition each of the color attachments and save information about them.
  std::vector<VkRenderingAttachmentInfoKHR> color;
  color.reserve(colorAttachments.size());
  for (auto& attachment : colorAttachments) {
    attachment.texture->_pimpl->barrier(frame->cmd,
      VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    VkClearColorValue clearColor {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
    if (attachment.clear) {
      clearColor.float32[0] = attachment.clear->at(0);
      clearColor.float32[1] = attachment.clear->at(1);
      clearColor.float32[2] = attachment.clear->at(2);
      clearColor.float32[3] = attachment.clear->at(3);
    }
    color.push_back(VkRenderingAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = attachment.texture->_pimpl->view,
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      .loadOp = (attachment.clear) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {clearColor} });
    viewportExtent.width = std::min<uint32_t>(viewportExtent.width, attachment.texture->_pimpl->extent.width);
    viewportExtent.height = std::min<uint32_t>(viewportExtent.height, attachment.texture->_pimpl->extent.height);
  }

  VkClearValue depthClear {.depthStencil = {.depth = 0.0f, .stencil = 0}};
  VkRenderingAttachmentInfo depth {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};

  // Transition the depth attachment and save information about it.
  if (depthAttachment) {
    depthAttachment->texture->_pimpl->barrier(frame->cmd,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, 
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    if (depthAttachment->clear) {
      depthClear.depthStencil.depth = depthAttachment->clear->depth;
      depthClear.depthStencil.stencil = depthAttachment->clear->stencil;
    }
    depth.imageView = depthAttachment->texture->_pimpl->view;
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = (depthAttachment->clear) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.clearValue = depthClear;
    viewportExtent.width = std::min<uint32_t>(viewportExtent.width, depthAttachment->texture->_pimpl->extent.width);
    viewportExtent.height = std::min<uint32_t>(viewportExtent.height, depthAttachment->texture->_pimpl->extent.height);
  }

  // Assemble the rendering info and begin rendering.
  VkRenderingInfo info {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = {
      .offset = { .x = 0, .y = 0 },
      .extent = viewportExtent },
      .layerCount = 1,
      .colorAttachmentCount = (uint32_t)color.size(),
      .pColorAttachments = color.data(),
      .pDepthAttachment = (depthAttachment) ? &depth : nullptr };
  vkCmdBeginRendering(frame->cmd, &info);
  frame->inDrawingPass = true;

  // Set the viewport.
  VkViewport view {
    .x = 0.f,
    .y = 0.f,
    .width = (float)viewportExtent.width,
    .height = (float)viewportExtent.height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f };
  vkCmdSetViewport(frame->cmd, 0, 1, &view);

  // Set the scissor.
  VkRect2D scissor {
    .offset = { .x = 0, .y = 0 },
    .extent = viewportExtent };
  vkCmdSetScissor(frame->cmd, 0, 1, &scissor);
}

void hlgl::endDrawing() {
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'endDrawing' outside of a frame.");
    return;
  }

  if (frame->inDrawingPass) {
    vkCmdEndRendering(frame->cmd);
    frame->inDrawingPass = false;
  }
}

void hlgl::bindPipeline(Pipeline* pipeline) {
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'bindPipeline' outside of a frame.");
    return;
  }

  if (frame->boundPipeline == pipeline)
    return;
  
  vkCmdBindPipeline(frame->cmd, pipeline->_pimpl->bindPoint, pipeline->_pimpl->pipeline);
  frame->boundPipeline = pipeline;
}

void hlgl::pushConstants(const void* data, size_t size) {
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'pushConstants' outside of a frame.");
    return;
  }

  if (!frame->boundPipeline) {
    DEBUG_ERROR("A pipeline must be bound before constants can be pushed to it.");
    return; }
  if (!data || !size) {
    DEBUG_ERROR("No constants data to push.");
    return; }

  vkCmdPushConstants(frame->cmd, getPipelineLayout(), VK_SHADER_STAGE_ALL, 0, (uint32_t)size, data);
}

void hlgl::dispatch(
  uint32_t groupCountX,
  uint32_t groupCountY,
  uint32_t groupCountZ)
{
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'dispatch' outside of a frame.");
    return;
  }

  if (!frame->boundPipeline || !frame->boundPipeline->isCompute()) {
    DEBUG_ERROR("A compute pipeline must be bound before calling 'dispatch'.");
    return;
  }
  vkCmdDispatch(frame->cmd, groupCountX, groupCountY, groupCountZ);
}

void hlgl::draw(
  uint32_t vertexCount,
  uint32_t instanceCount,
  uint32_t firstVertex,
  uint32_t firstInstance)
{
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'draw' outside of a frame.");
    return;
  }

  if (!frame->boundPipeline || !frame->boundPipeline->isGraphics()) {
    DEBUG_ERROR("A graphics pipeline must be bound before calling 'draw'.");
    return;
  }
  vkCmdDraw(frame->cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

void hlgl::drawIndexed(
  uint32_t indexCount,
  Buffer* indexBuffer,
  uint8_t indexSize,
  DeviceSize offset,
  uint32_t instanceCount,
  uint32_t firstIndex,
  uint32_t vertexOffset,
  uint32_t firstInstance)
{
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'drawIndexed' outside of a frame.");
    return;
  }

  if (!frame->boundPipeline || !frame->boundPipeline->isGraphics()) {
    DEBUG_ERROR("A graphics pipeline must be bound before calling 'drawIndexed'.");
    return;
  }

  if (frame->boundIndexBuffer != indexBuffer) {
    vkCmdBindIndexBuffer(frame->cmd, indexBuffer->_pimpl->getBuffer(frame), offset, translateIndexType(indexSize));
    frame->boundIndexBuffer = indexBuffer;
  }

  vkCmdDrawIndexed(frame->cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void hlgl::drawIndirect(
  Buffer* drawBuffer,
  DeviceSize drawOffset,
  uint32_t drawCount,
  uint32_t stride)
{
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'drawIndexed' outside of a frame.");
    return;
  }

  if (!frame->boundPipeline || !frame->boundPipeline->isGraphics()) {
    DEBUG_ERROR("A graphics pipeline must be bound before calling 'drawIndexed'.");
    return;
  }
  
  vkCmdDrawIndirect(frame->cmd, drawBuffer->_pimpl->getBuffer(frame), drawOffset, drawCount, stride);
}

void hlgl::drawIndexedIndirect(
  Buffer* drawBuffer,
  DeviceSize drawOffset,
  uint32_t drawCount,
  uint32_t stride)
{
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'drawIndexed' outside of a frame.");
    return;
  }

  if (!frame->boundPipeline || !frame->boundPipeline->isGraphics()) {
    DEBUG_ERROR("A graphics pipeline must be bound before calling 'drawIndexed'.");
    return;
  }

  vkCmdDrawIndexedIndirect(frame->cmd, drawBuffer->_pimpl->getBuffer(frame), drawOffset, drawCount, stride);
}

void hlgl::drawIndirectCount(
  Buffer* drawBuffer,
  DeviceSize drawOffset,
  Buffer* countBuffer,
  DeviceSize countOffset,
  uint32_t maxDraws,
  uint32_t stride)
{
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'drawIndexed' outside of a frame.");
    return;
  }

  if (!frame->boundPipeline || !frame->boundPipeline->isGraphics()) {
    DEBUG_ERROR("A graphics pipeline must be bound before calling 'drawIndexed'.");
    return;
  }
  
  vkCmdDrawIndirectCount(frame->cmd,
    drawBuffer->_pimpl->getBuffer(frame), drawOffset,
    countBuffer->_pimpl->getBuffer(frame), countOffset,
    maxDraws, stride);
}

void hlgl::drawIndexedIndirectCount(
  Buffer* drawBuffer,
  DeviceSize drawOffset,
  Buffer* countBuffer,
  DeviceSize countOffset,
  uint32_t maxDraws,
  uint32_t stride)
{
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'drawIndexed' outside of a frame.");
    return;
  }

  if (!frame->boundPipeline || !frame->boundPipeline->isGraphics()) {
    DEBUG_ERROR("A graphics pipeline must be bound before calling 'drawIndexed'.");
    return;
  }

  vkCmdDrawIndexedIndirectCount(frame->cmd,
    drawBuffer->_pimpl->getBuffer(frame), drawOffset,
    countBuffer->_pimpl->getBuffer(frame), countOffset,
    maxDraws, stride);
}

hlgl::Texture* hlgl::getFrameSwapchainImage() {
  Frame* frame {getCurrentFrame()};
  if (!frame) {
    DEBUG_ERROR("Can't call 'getFrameSwapchainImage()' outside of a frame.");
    return nullptr;
  }

  return frame->swapchainImage;
}