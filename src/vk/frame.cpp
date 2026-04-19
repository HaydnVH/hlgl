#include "frame.h"
#include "buffer.h"
#include "debug.h"
#include "pipeline.h"
#include "image.h"
#include "vulkan-translate.h"

void hlgl::blitImage(Frame* frame, Image* dst, Image* src, BlitRegion dstRegion, BlitRegion srcRegion, bool filterLinear) {

  // If we started a draw pass, end it here.
  endDrawing(frame);

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
        .x = (int)srcRegion.x,
        .y = (int)srcRegion.y,
        .z = (int)srcRegion.z },
        VkOffset3D{
        .x = (int)std::min(src->_pimpl->extent.width, srcRegion.x + srcRegion.w),
        .y = (int)std::min(src->_pimpl->extent.height, srcRegion.y + srcRegion.h),
        .z = (int)std::min(src->_pimpl->extent.depth, srcRegion.z + srcRegion.d)} },
      .dstSubresource = {
      .aspectMask = translateAspect(dst->getFormat()),
      .mipLevel = dstRegion.mipLevel,
      .baseArrayLayer = dstRegion.baseLayer,
      .layerCount = dstRegion.layerCount },
      .dstOffsets = {
        VkOffset3D{
        .x = (int)dstRegion.x,
        .y = (int)dstRegion.y,
        .z = (int)dstRegion.z },
        VkOffset3D{
        .x = (int)std::min(dst->_pimpl->extent.width, dstRegion.x + dstRegion.w),
        .y = (int)std::min(dst->_pimpl->extent.height, dstRegion.y + dstRegion.h),
        .z = (int)std::min(dst->_pimpl->extent.depth, dstRegion.z + dstRegion.d)} }
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

void hlgl::beginDrawing(Frame* frame, std::initializer_list<ColorAttachment> colorAttachments, std::optional<DepthAttachment> depthAttachment) {
  if (colorAttachments.size() <= 0) {
    debugPrint(DebugSeverity::Error, "beginDrawing requires at least one color attachment to output to.");
    return;
  }

  // If we started a draw pass, end it before starting a new one.
  endDrawing(frame);
  
  // Record the minimum extent of each attachment so we don't accidentally try to draw outside the framebuffers.
  VkExtent2D viewportExtent {};
  getDisplaySize(viewportExtent.width, viewportExtent.height);

  // Transition each of the color attachments and save information about them.
  std::vector<VkRenderingAttachmentInfoKHR> color;
  color.reserve(colorAttachments.size());
  for (auto& attachment : colorAttachments) {
    attachment.image->_pimpl->barrier(frame->cmd,
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
      .imageView = attachment.image->_pimpl->view,
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      .loadOp = (attachment.clear) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {clearColor} });
    viewportExtent.width = std::min(viewportExtent.width, attachment.image->_pimpl->extent.width);
    viewportExtent.height = std::min(viewportExtent.height, attachment.image->_pimpl->extent.height);
  }

  VkClearValue depthClear {.depthStencil = {.depth = 0.0f, .stencil = 0}};
  VkRenderingAttachmentInfo depth {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};

  // Transition the depth attachment and save information about it.
  if (depthAttachment) {
    depthAttachment->image->_pimpl->barrier(frame->cmd,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    if (depthAttachment->clear) {
      depthClear.depthStencil.depth = depthAttachment->clear->depth;
      depthClear.depthStencil.stencil = depthAttachment->clear->stencil;
    }
    depth.imageView = depthAttachment->image->_pimpl->view;
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = (depthAttachment->clear) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.clearValue = depthClear;
    viewportExtent.width = std::min(viewportExtent.width, depthAttachment->image->_pimpl->extent.width);
    viewportExtent.height = std::min(viewportExtent.height, depthAttachment->image->_pimpl->extent.height);
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

void hlgl::endDrawing(Frame* frame) {
  if (frame->inDrawingPass) {
    vkCmdEndRendering(frame->cmd);
    frame->inDrawingPass = false;
  }
}

void hlgl::bindPipeline(Frame* frame, Pipeline* pipeline) {
  if (frame->boundPipeline == pipeline)
    return;
  
  vkCmdBindPipeline(frame->cmd, pipeline->_pimpl->bindPoint_, pipeline->_pimpl->pipeline_);
  frame->boundPipeline = pipeline;
}

void hlgl::pushConstants(Frame* frame, const void* data, size_t size) {
  if (!frame->boundPipeline) {
    debugPrint(DebugSeverity::Error, "A pipeline must be bound before constants can be pushed to it.");
    return; }
  if (!data || !size) {
    debugPrint(DebugSeverity::Error, "No constants data to push.");
    return; }
  if (!frame->boundPipeline->_pimpl->pushConstRange_.stageFlags) {
    debugPrint(DebugSeverity::Error, "Bound pipeline doesn't have push constants.");
    return; }
  if (size != frame->boundPipeline->_pimpl->pushConstRange_.size) {
    debugPrint(DebugSeverity::Error, std::format(
      "Push constant size mismatch.  {} bytes provided, but pipeline expected {} bytes.",
      size, frame->boundPipeline->_pimpl->pushConstRange_.size));
    return; }

  vkCmdPushConstants(frame->cmd, frame->boundPipeline->_pimpl->layout_, frame->boundPipeline->_pimpl->pushConstRange_.stageFlags, 0, (uint32_t)size, data);
}

/*
void hlgl::pushBindings(std::initializer_list<Binding> bindings, bool barrier) {
  if (!boundPipeline_) { debugPrint(DebugSeverity::Error, "A pipeline must be bound before bindings can be pushed to it."); return; }
  if (bindings.size() == 0) { debugPrint(DebugSeverity::Error, "No bindings to push."); return; }
  if (boundPipeline_->descTypes_.size() == 0) { debugPrint(DebugSeverity::Error, "Bound pipeline doesn't have bindings."); return; }

  VkCommandBuffer cmd = _impl::getCurrentFrameCmdBuffer();

  std::vector<VkWriteDescriptorSet> descWrites;
  descWrites.reserve(bindings.size());
  std::vector<std::variant<VkDescriptorBufferInfo, VkDescriptorImageInfo>> descResourceInfos;
  descResourceInfos.reserve(bindings.size());

  uint32_t i {0};
  for (auto binding : bindings) {
    // We can have bind points higher than i, but we can't have an i higher than the highest bind point.  Bail out.
    if (i >= boundPipeline_->descTypes_.size())
      continue;
    descWrites.push_back(VkWriteDescriptorSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstBinding = i,
      .descriptorCount = 1,
      .descriptorType = boundPipeline_->descTypes_[i] });
    uint32_t bindIndex = getBindIndex(binding);
    if (bindIndex != UINT32_MAX) {
      descWrites.back().dstBinding = bindIndex;
      descWrites.back().descriptorType = boundPipeline_->descTypes_[bindIndex];
    }
    if (isBindBuffer(binding)) {
      auto bindBuffer = getBindBuffer(binding);
      if (!bindBuffer) { descWrites.pop_back(); continue; }
      if (barrier) {
        bindBuffer->barrier(cmd,
          ((isBindRead(binding)) ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_SHADER_WRITE_BIT),
          ((boundPipeline_->type_ == VK_PIPELINE_BIND_POINT_COMPUTE) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT) );
      }
      descResourceInfos.push_back(VkDescriptorBufferInfo{
        .buffer = bindBuffer->buffer_[bindBuffer->fifSynced_ ? _impl::getCurrentFrameIndex() : 0],
        .offset = 0,
        .range = bindBuffer->size_ });
      descWrites.back().pBufferInfo = &std::get<VkDescriptorBufferInfo>(descResourceInfos.back());
    }
    else if (isBindTexture(binding)) {
      auto bindTexture = getBindTexture(binding);
      if (!bindTexture) { descWrites.pop_back(); continue; }
      if (barrier) {
        bindTexture->_vk.barrier(cmd,
          (isBindRead(binding)) ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
          (isBindRead(binding)) ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_SHADER_WRITE_BIT,
          (boundPipeline_->type_ == VK_PIPELINE_BIND_POINT_COMPUTE) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT );
      }
      descResourceInfos.push_back(VkDescriptorImageInfo{
        .sampler = bindTexture->_vk.sampler,
        .imageView = bindTexture->_vk.view,
        .imageLayout = bindTexture->_vk.layout });
      descWrites.back().pImageInfo = &std::get<VkDescriptorImageInfo>(descResourceInfos.back());
    }
    else {
      // Binding variant is in an invalid state somehow.
      descWrites.pop_back();
      continue;
    }
    ++i;
  }
  vkCmdPushDescriptorSetKHR(cmd, boundPipeline_->type_, boundPipeline_->layout_, 0, (uint32_t)descWrites.size(), descWrites.data());
}
*/

void hlgl::dispatch(
  Frame* frame,
  uint32_t groupCountX,
  uint32_t groupCountY,
  uint32_t groupCountZ)
{
  if (!frame->boundPipeline || !frame->boundPipeline->isCompute()) {
    debugPrint(DebugSeverity::Error, "A compute pipeline must be bound before calling 'dispatch'.");
    return;
  }
  vkCmdDispatch(frame->cmd, groupCountX, groupCountY, groupCountZ);
}

void hlgl::draw(
  Frame* frame,
  uint32_t vertexCount,
  uint32_t instanceCount,
  uint32_t firstVertex,
  uint32_t firstInstance)
{
  if (!frame->boundPipeline || !frame->boundPipeline->isGraphics()) {
    debugPrint(DebugSeverity::Error, "A graphics pipeline must be bound before calling 'draw'.");
    return;
  }
  vkCmdDraw(frame->cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

void hlgl::bindIndexBuffer(Frame* frame, Buffer* indexBuffer, DeviceSize offset) {
  if (!frame->boundPipeline || !frame->boundPipeline->isGraphics()) {
    debugPrint(DebugSeverity::Error, "A graphics pipeline must be bound before calling 'bindIndexBuffer'.");
    return;
  }
  if (frame->boundIndexBuffer != indexBuffer) {
    vkCmdBindIndexBuffer(frame->cmd, indexBuffer->_pimpl->getBuffer(frame), offset, translateIndexType(indexBuffer->_pimpl->indexSize));
    frame->boundIndexBuffer = indexBuffer;
  }
}

void hlgl::drawIndexed(
  Frame* frame,
  uint32_t indexCount,
  uint32_t instanceCount,
  uint32_t firstIndex,
  uint32_t vertexOffset,
  uint32_t firstInstance)
{
  if (!frame->boundPipeline || !frame->boundPipeline->isGraphics()) {
    debugPrint(DebugSeverity::Error, "A graphics pipeline must be bound before calling 'drawIndexed'.");
    return;
  }
  if (!frame->boundIndexBuffer) {
    debugPrint(DebugSeverity::Error, "An index buffer must be bound before calling 'drawIndexed'.");
    return;
  }

  vkCmdDrawIndexed(frame->cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

hlgl::Image* hlgl::getFrameSwapchainImage(Frame* frame) {
  return frame->swapchainImage;
}