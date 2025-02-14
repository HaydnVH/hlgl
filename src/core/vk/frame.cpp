#include "vk-includes.h"
#include "vk-debug.h"
#include "vk-translate.h"
#include <hlgl/core/context.h>
#include <hlgl/core/frame.h>

#ifdef HLGL_INCLUDE_IMGUI
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#ifdef HLGL_WINDOW_LIBRARY_GLFW
#include <backends/imgui_impl_glfw.h>
#endif
#endif

hlgl::Frame::Frame(hlgl::Context& context)
  : context_(context)
{
  if (context_.inFrame_) {
    //debugPrint(DebugSeverity::Warning, "Cannot begin a frame while a frame is active!");
    return;
  }

  // Get the command buffer and sync structures for the current frame.
  auto& frame = context_.frames_[context_.frameIndex_];

  // Block until the previous commands sent to this frame are finished.
  if (!VKCHECK(vkWaitForFences(context_.device_, 1, &frame.fence, true, UINT64_MAX))) {
    return;
  }

  // Resize the swapchain if neccessary; if this returns false, that indicates that the current frame should not be drawn.
  if (!context_.resizeIfNeeded(context_.displayWidth_, context_.displayHeight_, context_.displayHdr_, context_.displayVsync_))
    return;

  // Reset the in-flight fence.
  if (!VKCHECK(vkResetFences(context_.device_, 1, &frame.fence)))
    return;

  // Get the next image index.
  if (!VKCHECK(vkAcquireNextImageKHR(context_.device_, context_.swapchain_, UINT64_MAX, frame.imageAvailable, nullptr, &context_.swapchainIndex_)))
    return;

  // Reset this frame's command buffer from its previous usage.
  if (!VKCHECK(vkResetCommandBuffer(frame.cmd, 0)))
    return;

  // Delete any objects that were destroyed on this frame after the command buffer's been reset.
  context_.flushDelQueue();

  // Begin recording commands.
  VkCommandBufferBeginInfo info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    //.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };
  if (!VKCHECK(vkBeginCommandBuffer(frame.cmd, &info)))
    return;

  context_.inFrame_ = true;
  initSuccess_ = true;
}

hlgl::Frame::~Frame() {
  if (!initSuccess_)
    return;

  // Get this frame and the current swapchain texture.
  auto& frame {context_.frames_[context_.frameIndex_]};
  auto texture {getSwapchainTexture()};

  // Draw the ImGUI frame to a custom drawing pass.
#ifdef HLGL_INCLUDE_IMGUI
  if (context_.gpu_.enabledFeatures & Feature::Imgui) {
    beginDrawing({{texture}});
    auto drawData = ImGui::GetDrawData();
    if (drawData)
      ImGui_ImplVulkan_RenderDrawData(drawData, frame.cmd, nullptr);
  }
#endif

  // If we started a draw pass, end it here.
  endDrawing();

  // Transition the swapchain image to a presentable state.
  texture->barrier(frame.cmd,
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
  if (!VKCHECK(vkQueueSubmit(context_.graphicsQueue_, 1, &si, frame.fence)))
    return;

  // Present the image to the screen.
  VkPresentInfoKHR pi {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &frame.renderFinished,
    .swapchainCount = 1,
    .pSwapchains = &context_.swapchain_,
    .pImageIndices = &context_.swapchainIndex_
  };
  if (!VKCHECK(vkQueuePresentKHR(context_.presentQueue_, &pi)))
    return;

  // Advance the frame index for the next frame.
  context_.frameIndex_ = (context_.frameIndex_ + 1) % context_.frames_.size();
  context_.inFrame_ = false;
}

uint32_t hlgl::Frame::getFrameIndex() const {
  return context_.frameIndex_;
}

void hlgl::Frame::blit(Texture& dst, Texture& src, BlitRegion dstRegion, BlitRegion srcRegion, bool filterLinear) {
  VkCommandBuffer cmd = context_.getCommandBuffer();

  // If we started a draw pass, end it here.
  if (inDrawPass_) {
    vkCmdEndRenderingKHR(cmd);
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
    dstRegion.x = 0; dstRegion.y = 0; dstRegion.z = 0;
    dstRegion.w = context_.displayWidth_; dstRegion.h = context_.displayHeight_; dstRegion.d = 1;
  }

  if (srcRegion.screenRegion) {
    srcRegion.x = 0; srcRegion.y = 0; srcRegion.z = 0;
    srcRegion.w = context_.displayWidth_; srcRegion.h = context_.displayHeight_; srcRegion.d = 1;
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

hlgl::Texture* hlgl::Frame::getSwapchainTexture() {
  return &context_.swapchainTextures_[context_.swapchainIndex_];
} 

void hlgl::Frame::beginDrawing(std::initializer_list<AttachColor> colorAttachments, std::optional<AttachDepthStencil> depthAttachment) {
  if (!initSuccess_)
    return;

  if (colorAttachments.size() <= 0) {
    debugPrint(DebugSeverity::Error, "beginDrawing requires at least one color attachment to output to.");
    return;
  }

  // If we started a draw pass, end it before starting a new one.
  endDrawing();

  VkCommandBuffer cmd = context_.getCommandBuffer();
  
  // Record the minimum extent of each attachment so we don't accidentally try to draw outside the framebuffers.
  VkExtent2D viewportExtent {context_.swapchainExtent_.width, context_.swapchainExtent_.height};

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
      .clearValue = clearColor });
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
    vkCmdEndRenderingKHR(context_.getCommandBuffer());
    inDrawPass_ = false;
  }
}

void hlgl::Frame::bindPipeline(const Pipeline* pipeline) {
  if (boundPipeline_ == pipeline) { return; }
  if (!pipeline || !pipeline->isValid()) { debugPrint(DebugSeverity::Warning, "Cannot bind invalid pipeline."); return; }

  vkCmdBindPipeline(context_.getCommandBuffer(), pipeline->type_, pipeline->pipeline_);
  boundPipeline_ = pipeline;
}

void hlgl::Frame::pushConstants(const void* data, size_t size) {
  if (!boundPipeline_) { debugPrint(DebugSeverity::Error, "A pipeline must be bound before constants can be pushed to it."); return; }
  if (!data || !size) { debugPrint(DebugSeverity::Error, "No constants data to push."); return; }
  if (!boundPipeline_->pushConstRange_.stageFlags) { debugPrint(DebugSeverity::Error, "Bound pipeline doesn't have push constants."); return; }
  if (size != boundPipeline_->pushConstRange_.size) { debugPrint(DebugSeverity::Error, fmt::format("Push constant size mismatch.  {} bytes provided, but pipeline expected {} bytes.", size, boundPipeline_->pushConstRange_.size)); return; }

  vkCmdPushConstants(context_.getCommandBuffer(), boundPipeline_->layout_, boundPipeline_->pushConstRange_.stageFlags, 0, (uint32_t)size, data);
}

void hlgl::Frame::pushBindings(std::initializer_list<Binding> bindings, bool barrier) {
  if (!boundPipeline_) { debugPrint(DebugSeverity::Error, "A pipeline must be bound before bindings can be pushed to it."); return; }
  if (bindings.size() == 0) { debugPrint(DebugSeverity::Error, "No bindings to push."); return; }
  if (boundPipeline_->descTypes_.size() == 0) { debugPrint(DebugSeverity::Error, "Bound pipeline doesn't have bindings."); return; }

  VkCommandBuffer cmd = context_.getCommandBuffer();

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
        .buffer = bindBuffer->buffer_[bindBuffer->fifSynced_ ? getFrameIndex() : 0],
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
  vkCmdDispatch(context_.getCommandBuffer(), groupCountX, groupCountY, groupCountZ);
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
  vkCmdDraw(context_.getCommandBuffer(), vertexCount, instanceCount, firstVertex, firstInstance);
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
  VkCommandBuffer cmd = context_.getCommandBuffer();

  if (indexBuffer != boundIndexBuffer_) {
    vkCmdBindIndexBuffer(cmd, indexBuffer->buffer_[indexBuffer->fifSynced_ ? getFrameIndex() : 0], 0, translateIndexType(indexBuffer->indexSize_));
    boundIndexBuffer_ = indexBuffer;
  }
  vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}
