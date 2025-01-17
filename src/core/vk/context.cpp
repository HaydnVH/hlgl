#include "vk-includes.h"
#include "vk-debug.h"
#include "vk-translate.h"
#include <hlgl/core/context.h>
#include <hlgl/core/frame.h>

#include <algorithm>

#if defined HLGL_WINDOW_LIBRARY_GLFW
#include <GLFW/glfw3.h>
#endif // defined HLGL_WINDOW_LIBRARY_x

#ifdef HLGL_INCLUDE_IMGUI
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#ifdef HLGL_WINDOW_LIBRARY_GLFW
#include <backends/imgui_impl_glfw.h>
#endif
#endif


hlgl::Context::Context(ContextParams params) {

  // Uniqueness check.
  static bool alreadyCreated {false};
  if (alreadyCreated) {
    debugPrint(DebugSeverity::Error, "HLGL Context cannot be created more than once.");
    return;
  }
  else
    alreadyCreated = true;

  setDebugCallback(params.fnDebugCallback);

  // Features which are required are also preferred, the user need not repeat themselves.
  params.preferredFeatures |= params.requiredFeatures;

  // Check for ImGui support.
#ifndef HLGL_INCLUDE_IMGUI
  if (params.requiredFeatures & Feature::Imgui) {
    debugPrint(DebugSeverity::Error, "HLGL was not compiled with ImGui support enabled but ImGui was set as a required feature.");
    return;
  }
#else
  gpu_.supportedFeatures |= Feature::Imgui;
#endif

  // Get the window dimensions.
#ifdef HLGL_WINDOW_LIBRARY_GLFW
  {
    int w{0}, h{0};
    glfwGetWindowSize(params.pWindow, &w, &h);
    displayWidth_ = w;
    displayHeight_ = h;
  }
#endif

  displayHdr_ = (params.preferredFeatures & Feature::DisplayHdr);
  displayVsync_ = (params.preferredFeatures & Feature::DisplayVsync);

  // Initialize vulkan backend.

  if (!initInstance(VkApplicationInfo{
    .pApplicationName = params.sAppName,
    .applicationVersion = VK_MAKE_VERSION(params.appVersion.major, params.appVersion.minor, params.appVersion.patch),
    .pEngineName = params.sEngineName,
    .engineVersion = VK_MAKE_VERSION(params.engineVersion.major, params.engineVersion.minor, params.engineVersion.patch),
    .apiVersion = VK_API_VERSION_1_3},
    params.preferredFeatures, params.requiredFeatures)) return;
  if ((params.fnDebugCallback) && (params.preferredFeatures & Feature::Validation))
    { if (!initDebug()) return; }
  if (!initSurface(params.pWindow)) return;
  if (!pickPhysicalDevice(params.sPreferredGpu, params.preferredFeatures, params.requiredFeatures)) return;
  if (!initDevice(params.preferredFeatures, params.requiredFeatures)) return;
  if (!initQueues()) return;
  if (!initCmdPool()) return;
  if (!initDescPool()) return;
  if (!resizeSwapchain()) return;
  if (!initFrames()) return;
  if (!initAllocator()) return;
  if (params.preferredFeatures & Feature::Imgui)
    { if (!initImGui(params.pWindow)) return; }

  debugPrint(DebugSeverity::Debug, "Finished initializing HLGL context.");

  initSuccess_ = true;
}

hlgl::Context::~Context() {
  if (device_)
    vkDeviceWaitIdle(device_);

  destroyBackend();
}

hlgl::Format hlgl::Context::getDisplayFormat() {
  return translate(swapchainFormat_);
}

bool hlgl::Context::resizeIfNeeded(uint32_t width, uint32_t height, bool hdr, bool vsync) {
  VkExtent2D checkExtent;
  VkSurfaceCapabilitiesKHR caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);
  if (caps.currentExtent.width != UINT32_MAX && caps.currentExtent.height != UINT32_MAX)
    checkExtent = caps.currentExtent;
  else {
    checkExtent.width = std::clamp<uint32_t>(width, caps.minImageExtent.width, caps.maxImageExtent.width);
    checkExtent.height = std::clamp<uint32_t>(height, caps.minImageExtent.height, caps.maxImageExtent.height);
  }

  // If the display size has been changed, we'll have to recreate the swapchain.
  if ((checkExtent.width != swapchainExtent_.width) ||
      (checkExtent.height != swapchainExtent_.height) ||
      (hdr != displayHdr_) ||
      (vsync != displayVsync_))
  {
    // If the window has 0 width or height, bail out here.
    if (checkExtent.width == 0 || checkExtent.height == 0)
      return false;

    // Recreate the swapchain.
    resizeSwapchain();

    // Skip the frame when the swapchain is resized.
    return false;
  }
  // The swapchain hasn't been resized, but we'll check for 0 just in case.
  else if (swapchainExtent_.width == 0 || swapchainExtent_.height == 0)
    return false;

  // The swapchain hasn't been resized and its size is non-zero, so we're good to go.
  return true;
}

void hlgl::Context::immediateSubmit(const std::function<void(VkCommandBuffer)>& func) const {
  VkCommandBuffer cmd {nullptr};
  VkCommandBufferAllocateInfo ai {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = cmdPool_,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1 };
  if (!VKCHECK(vkAllocateCommandBuffers(device_, &ai, &cmd)) || !cmd)
    return;

  VkCommandBufferBeginInfo bi {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
  vkBeginCommandBuffer(cmd, &bi);
  func(cmd);
  vkEndCommandBuffer(cmd);

  VkSubmitInfo si {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd };
  VKCHECK(vkQueueSubmit(graphicsQueue_, 1, &si, nullptr));
  VKCHECK(vkQueueWaitIdle(graphicsQueue_));
  vkFreeCommandBuffers(device_, cmdPool_, 1, &cmd);
}

void hlgl::Context::imguiNewFrame() {
#ifdef HLGL_INCLUDE_IMGUI
  ImGui_ImplVulkan_NewFrame();
#ifdef HLGL_WINDOW_LIBRARY_GLFW
  ImGui_ImplGlfw_NewFrame();
#endif
  ImGui::NewFrame();
#endif
}

hlgl::Frame hlgl::Context::beginFrame() {
  return Frame(*this);
}