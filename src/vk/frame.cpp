#include "vkimpl-includes.h"
#include "vkimpl-debug.h"
#include "vkimpl-translate.h"
#include "vkimpl-context.h"
#include <hlgl/context.h>
#include <hlgl/frame.h>

namespace {
  bool inFrame_s {false};
}

hlgl::Frame::Frame()
{
  if (inFrame_s) {
    return;
  }

  // Get the command buffer and sync structures for the current frame.
  _impl::FrameInFlight& frame = _impl::getCurrentFrameInFlight();

  // Block until the previous commands sent to this frame are finished.
  if (!VKCHECK(vkWaitForFences(_impl::getDevice(), 1, &frame.fence, true, UINT64_MAX))) {
    return;
  }

  // Resize the swapchain if neccessary; if this returns false, that indicates that the current frame should not be drawn.
  if (!_impl::resizeIfNeeded())
    return;

  // Reset the in-flight fence.
  if (!VKCHECK(vkResetFences(_impl::getDevice(), 1, &frame.fence)))
    return;

  // Get the next image index.
  if (!_impl::acquireNextImage())
    return;

  // Reset this frame's command buffer from its previous usage.
  if (!VKCHECK(vkResetCommandBuffer(frame.cmd, 0)))
    return;

  // Delete any objects that were destroyed on this frame after the command buffer's been reset.
  _impl::flushDelQueue();

  // Begin recording commands.
  VkCommandBufferBeginInfo info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    //.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };
  if (!VKCHECK(vkBeginCommandBuffer(frame.cmd, &info)))
    return;

  initSuccess_ = true;
  inFrame_s = true;
}

hlgl::Frame::~Frame() {
  if (!initSuccess_)
    return;

  inFrame_s = false;
  
  // Get this frame and the current swapchain texture.
  _impl::FrameInFlight& frame {_impl::getCurrentFrameInFlight()};
  Texture& texture {_impl::getCurrentSwapchainTexture()};

  // If we started a draw pass, end it here.
  endDrawing();

  // Draw the ImGUI frame to a custom drawing pass.
  beginDrawing({{&texture}});
  ImDrawData* drawData = ImGui::GetDrawData();
  if (drawData)
    ImGui_ImplVulkan_RenderDrawData(drawData, frame.cmd, nullptr);
  endDrawing();

  // Transition the swapchain image to a presentable state.
  texture.barrier(frame.cmd,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    VK_ACCESS_NONE,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

  // End the command buffer.
  vkEndCommandBuffer(frame.cmd);

  // Submit the command buffer to the graphics queue.
  VkPipelineStageFlags waitStages {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSubmitInfo si {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &frame.imageAvailable,
    .pWaitDstStageMask = &waitStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &frame.cmd,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &frame.renderFinished };
  if (!VKCHECK(vkQueueSubmit(_impl::getGraphicsQueue(), 1, &si, frame.fence)))
    return;

  VkSwapchainKHR swapchain {_impl::getSwapchain()};
  uint32_t swapchainIndex {_impl::getSwapchainIndex()};

  // Present the image to the screen.
  VkPresentInfoKHR pi {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &frame.renderFinished,
    .swapchainCount = 1,
    .pSwapchains = &swapchain,
    .pImageIndices = &swapchainIndex
  };
  if (!VKCHECK_SWAPCHAIN(vkQueuePresentKHR(_impl::getPresentQueue(), &pi)))
    return;

  // Advance the frame index for the next frame.
  _impl::advanceFrame();
}

void hlgl::Frame::blit(Texture& dst, Texture& src, BlitRegion dstRegion, BlitRegion srcRegion, bool filterLinear) {
  VkCommandBuffer cmd = _impl::getCurrentFrameInFlight().cmd;

  // If we started a draw pass, end it here.
  if (inDrawPass_) {
    vkCmdEndRendering(cmd);
    inDrawPass_ = false;
  }

  // Barrier transition the blit source.
  src.barrier(cmd,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    VK_ACCESS_TRANSFER_READ_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Barrier transition the blit destination.
  dst.barrier(cmd,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_ACCESS_TRANSFER_WRITE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT);

  if (dstRegion.screenRegion) {
    dstRegion.x = 0; dstRegion.y = 0; dstRegion.z = 0; dstRegion.d = 1;
    context::getDisplaySize(dstRegion.w, dstRegion.h);
  }

  if (srcRegion.screenRegion) {
    srcRegion.x = 0; srcRegion.y = 0; srcRegion.z = 0;  srcRegion.d = 1;
    context::getDisplaySize(srcRegion.w, srcRegion.h);
  }

  VkImageBlit2 region {
    .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
    .srcSubresource = {
      .aspectMask = translateAspect(src.format_),
      .mipLevel = srcRegion.mipLevel,
      .baseArrayLayer = srcRegion.baseLayer,
      .layerCount = srcRegion.layerCount },
      .srcOffsets = {
        VkOffset3D{
        .x = (int)srcRegion.x,
        .y = (int)srcRegion.y,
        .z = (int)srcRegion.z },
        VkOffset3D{
        .x = (int)std::min(src.extent_.width, srcRegion.x + srcRegion.w),
        .y = (int)std::min(src.extent_.height, srcRegion.y + srcRegion.h),
        .z = (int)std::min(src.extent_.depth, srcRegion.z + srcRegion.d)} },
      .dstSubresource = {
      .aspectMask = translateAspect(dst.format_),
      .mipLevel = dstRegion.mipLevel,
      .baseArrayLayer = dstRegion.baseLayer,
      .layerCount = dstRegion.layerCount },
      .dstOffsets = {
        VkOffset3D{
        .x = (int)dstRegion.x,
        .y = (int)dstRegion.y,
        .z = (int)dstRegion.z },
        VkOffset3D{
        .x = (int)std::min(dst.extent_.width, dstRegion.x + dstRegion.w),
        .y = (int)std::min(dst.extent_.height, dstRegion.y + dstRegion.h),
        .z = (int)std::min(dst.extent_.depth, dstRegion.z + dstRegion.d)} }
  };

  VkBlitImageInfo2 info {
    .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
    .srcImage = src.image_,
    .srcImageLayout = src.layout_,
    .dstImage = dst.image_,
    .dstImageLayout = dst.layout_,
    .regionCount = 1,
    .pRegions = &region,
    .filter = filterLinear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
  };
  vkCmdBlitImage2(cmd, &info);
}

hlgl::Texture& hlgl::Frame::getSwapchainTexture() {
  return _impl::getCurrentSwapchainTexture();
} 

void hlgl::Frame::beginDrawing(std::initializer_list<ColorAttachment> colorAttachments, std::optional<DepthStencilAttachment> depthAttachment) {
  if (!initSuccess_)
    return;

  if (colorAttachments.size() <= 0) {
    debugPrint(DebugSeverity::Error, "beginDrawing requires at least one color attachment to output to.");
    return;
  }

  // If we started a draw pass, end it before starting a new one.
  endDrawing();

  VkCommandBuffer cmd = _impl::getCurrentFrameInFlight().cmd;
  
  // Record the minimum extent of each attachment so we don't accidentally try to draw outside the framebuffers.
  VkExtent2D viewportExtent {};
  context::getDisplaySize(viewportExtent.width, viewportExtent.height);

  // Transition each of the color attachments and save information about them.
  std::vector<VkRenderingAttachmentInfoKHR> color;
  color.reserve(colorAttachments.size());
  for (auto& attachment : colorAttachments) {
    attachment.texture->barrier(cmd,
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
      .imageView = attachment.texture->view_,
      .imageLayout = attachment.texture->layout_,
      .loadOp = (attachment.clear) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {clearColor} });
    viewportExtent.width = std::min(viewportExtent.width, attachment.texture->extent_.width);
    viewportExtent.height = std::min(viewportExtent.height, attachment.texture->extent_.height);
  }

  VkClearValue depthClear {.depthStencil = {.depth = 0.0f, .stencil = 0}};
  VkRenderingAttachmentInfo depth {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};

  // Transition the depth attachment and save information about it.
  if (depthAttachment) {
    depthAttachment->texture->barrier(cmd,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    if (depthAttachment->clear) {
      depthClear.depthStencil.depth = depthAttachment->clear->depth;
      depthClear.depthStencil.stencil = depthAttachment->clear->stencil;
    }
    depth.imageView = depthAttachment->texture->view_;
    depth.imageLayout = depthAttachment->texture->layout_;
    depth.loadOp = (depthAttachment->clear) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.clearValue = depthClear;
    viewportExtent.width = std::min(viewportExtent.width, depthAttachment->texture->extent_.width);
    viewportExtent.height = std::min(viewportExtent.height, depthAttachment->texture->extent_.height);
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
  vkCmdBeginRendering(cmd, &info);
  inDrawPass_ = true;

  // Set the viewport.
  VkViewport view {
    .x = 0.f,
    .y = 0.f,
    .width = (float)viewportExtent.width,
    .height = (float)viewportExtent.height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f };
  vkCmdSetViewport(cmd, 0, 1, &view);

  // Set the scissor.
  VkRect2D scissor {
    .offset = { .x = 0, .y = 0 },
    .extent = viewportExtent };
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  viewportWidth_ = viewportExtent.width;
  viewportHeight_ = viewportExtent.height;
}

void hlgl::Frame::endDrawing() {
  if (inDrawPass_) {
    vkCmdEndRendering(_impl::getCurrentFrameInFlight().cmd);
    inDrawPass_ = false;
  }
}

void hlgl::Frame::bindPipeline(const Pipeline* pipeline) {
  if (boundPipeline_ == pipeline) { return; }
  if (!pipeline || !pipeline->isValid()) { debugPrint(DebugSeverity::Warning, "Cannot bind invalid pipeline."); return; }

  vkCmdBindPipeline(_impl::getCurrentFrameInFlight().cmd, pipeline->type_, pipeline->pipeline_);
  boundPipeline_ = pipeline;
}

void hlgl::Frame::pushConstants(const void* data, size_t size) {
  if (!boundPipeline_) { debugPrint(DebugSeverity::Error, "A pipeline must be bound before constants can be pushed to it."); return; }
  if (!data || !size) { debugPrint(DebugSeverity::Error, "No constants data to push."); return; }
  if (!boundPipeline_->pushConstRange_.stageFlags) { debugPrint(DebugSeverity::Error, "Bound pipeline doesn't have push constants."); return; }
  if (size != boundPipeline_->pushConstRange_.size) { debugPrint(DebugSeverity::Error, std::format("Push constant size mismatch.  {} bytes provided, but pipeline expected {} bytes.", size, boundPipeline_->pushConstRange_.size)); return; }

  vkCmdPushConstants(_impl::getCurrentFrameInFlight().cmd, boundPipeline_->layout_, boundPipeline_->pushConstRange_.stageFlags, 0, (uint32_t)size, data);
}

void hlgl::Frame::pushBindings(std::initializer_list<Binding> bindings, bool barrier) {
  if (!boundPipeline_) { debugPrint(DebugSeverity::Error, "A pipeline must be bound before bindings can be pushed to it."); return; }
  if (bindings.size() == 0) { debugPrint(DebugSeverity::Error, "No bindings to push."); return; }
  if (boundPipeline_->descTypes_.size() == 0) { debugPrint(DebugSeverity::Error, "Bound pipeline doesn't have bindings."); return; }

  VkCommandBuffer cmd = _impl::getCurrentFrameInFlight().cmd;

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
        .buffer = bindBuffer->buffer_[bindBuffer->fifSynced_ ? _impl::getFrameIndex() : 0],
        .offset = 0,
        .range = bindBuffer->size_ });
      descWrites.back().pBufferInfo = &std::get<VkDescriptorBufferInfo>(descResourceInfos.back());
    }
    else if (isBindTexture(binding)) {
      auto bindTexture = getBindTexture(binding);
      if (!bindTexture) { descWrites.pop_back(); continue; }
      if (barrier) {
        bindTexture->barrier(cmd,
          (isBindRead(binding)) ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
          (isBindRead(binding)) ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_SHADER_WRITE_BIT,
          (boundPipeline_->type_ == VK_PIPELINE_BIND_POINT_COMPUTE) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT );
      }
      descResourceInfos.push_back(VkDescriptorImageInfo{
        .sampler = bindTexture->sampler_,
        .imageView = bindTexture->view_,
        .imageLayout = bindTexture->layout_ });
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

void hlgl::Frame::dispatch(
  uint32_t groupCountX,
  uint32_t groupCountY,
  uint32_t groupCountZ)
{
  if (!boundPipeline_ || (boundPipeline_->type_ != VK_PIPELINE_BIND_POINT_COMPUTE)) {
    debugPrint(DebugSeverity::Error, "A compute pipeline must be bound before calling 'dispatch'.");
    return;
  }
  vkCmdDispatch(_impl::getCurrentFrameInFlight().cmd, groupCountX, groupCountY, groupCountZ);
}

void hlgl::Frame::draw(
  uint32_t vertexCount,
  uint32_t instanceCount,
  uint32_t firstVertex,
  uint32_t firstInstance)
{
  if (!boundPipeline_ || (boundPipeline_->type_ != VK_PIPELINE_BIND_POINT_GRAPHICS)) {
    debugPrint(DebugSeverity::Error, "A graphics pipeline must be bound before calling 'draw'.");
    return;
  }
  vkCmdDraw(_impl::getCurrentFrameInFlight().cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

void hlgl::Frame::drawIndexed(
  Buffer* indexBuffer,
  uint32_t indexCount,
  uint32_t instanceCount,
  uint32_t firstIndex,
  uint32_t vertexOffset,
  uint32_t firstInstance)
{
  if (!boundPipeline_ || !(boundPipeline_->type_ == VK_PIPELINE_BIND_POINT_GRAPHICS)) {
    debugPrint(DebugSeverity::Error, "A graphics pipeline must be bound before calling 'drawIndexed'.");
    return;
  }
  if (!indexBuffer || !indexBuffer->isValid()) {
    debugPrint(DebugSeverity::Error, "A non-null index buffer is required for 'drawIndexed'.");
    return;
  }
  VkCommandBuffer cmd = _impl::getCurrentFrameInFlight().cmd;

  if (indexBuffer != boundIndexBuffer_) {
    vkCmdBindIndexBuffer(cmd, indexBuffer->buffer_[indexBuffer->fifSynced_ ? _impl::getFrameIndex() : 0], 0, translateIndexType(indexBuffer->indexSize_));
    boundIndexBuffer_ = indexBuffer;
  }
  vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}
