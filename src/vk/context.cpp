#include "context.h"
#include "buffer.h"
#include "texture.h"
#include "frame.h"

#include "../utils/array.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <vector>

#if defined HLGL_WINDOW_LIBRARY_GLFW
#include <GLFW/glfw3.h>
#elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
#include <Windows.h>
#endif // defined HLGL_WINDOW_LIBRARY_x

namespace {

  hlgl::DebugCallbackFunc debugCallback_s {};
  bool initSuccess_s {false};
  hlgl::GpuProperties gpu_s;
  uint32_t displayWidth_s {0}, displayHeight_s {0};
  hlgl::VsyncMode vsync_s {hlgl::VsyncMode::Fifo};
  bool hdr_s {false};
  
  VkInstance instance_s {nullptr};
  VkDebugUtilsMessengerEXT debug_s {nullptr};
  VkSurfaceKHR surface_s {nullptr};
  VkPhysicalDevice physicalDevice_s {nullptr};
  VkDevice device_s {nullptr};
  VmaAllocator allocator_s {nullptr};

  uint32_t graphicsQueueFamily_s {UINT32_MAX};
  uint32_t presentQueueFamily_s {UINT32_MAX};
  uint32_t computeQueueFamily_s {UINT32_MAX};
  uint32_t transferQueueFamily_s {UINT32_MAX};
  VkQueue graphicsQueue_s {nullptr};
  VkQueue presentQueue_s {nullptr};
  VkQueue computeQueue_s {nullptr};
  VkQueue transferQueue_s {nullptr};

  VkCommandPool cmdPoolGraphics_s {nullptr};
  constexpr size_t numFramesInFlight_c {2};
  std::array<VkCommandBuffer, numFramesInFlight_c> frameCmdBuffers_s;
  std::array<VkFence, numFramesInFlight_c> frameFences_s;
  std::array<VkSemaphore, numFramesInFlight_c> acquireSemaphores_s;
  uint32_t frameIndex_s {0};
  uint64_t frameCounter_s {0};
  bool inFrame_s {false};
  hlgl::Frame frame_s {};

  std::array<VkDescriptorSetLayout, hlgl::NUM_DESCRIPTOR_SETS> descLayouts_s {};
  std::array<VkDescriptorSet, hlgl::NUM_DESCRIPTOR_SETS> descSets_s {};
  std::array<uint32_t, hlgl::NUM_DESCRIPTOR_SETS> descNextIndex_s {0,0,0};
  std::array<std::vector<uint32_t>, hlgl::NUM_DESCRIPTOR_SETS> descFreeIndices_s {};
  VkDescriptorPool descPool_s {nullptr};
  VkPipelineLayout pipeLayout_s {nullptr};

  std::optional<hlgl::Texture> defaultTextureNull_s  {std::nullopt};
  std::optional<hlgl::Texture> defaultTextureWhite_s {std::nullopt};
  std::optional<hlgl::Texture> defaultTextureGray_s  {std::nullopt};
  std::optional<hlgl::Texture> defaultTextureBlack_s {std::nullopt};

  VkCommandPool cmdPoolTransfer_s {nullptr};
  VkCommandBuffer cmdTransfer_s {nullptr};
  std::optional<hlgl::Buffer> stagingBuffer_s {std::nullopt};
  hlgl::DeviceSize stagingBufferOffset_s {0};
  uint64_t stagingBufferLastFrameUsed_s {0};

  VkSwapchainKHR swapchain_s {nullptr};
  VkExtent2D swapchainExtent_s {};
  VkFormat swapchainFormat_s {};
  VkPresentModeKHR swapchainPresentMode_s {VK_PRESENT_MODE_FIFO_KHR};
  uint32_t swapchainIndex_s {0};
  bool swapchainNeedsRebuild_s {false};
  std::vector<hlgl::Texture> swapchainImages_s {};
  std::vector<VkSemaphore> submitSemaphores_s {};
  hlgl::Observable<uint32_t,uint32_t> subjectDisplayResized_s {};  

  constexpr size_t numDelQueues_c {3};
  std::array<std::vector<hlgl::DelQueueItem>, numDelQueues_c> delQueues_s;

  bool isLayerSupported(const std::vector<VkLayerProperties>& layerProperties, const std::string_view requestedlayer) {
    for (const VkLayerProperties& layer : layerProperties) {
      if (requestedlayer == layer.layerName)
        return true;
    }
    return false;
  }

  bool isExtensionSupported(const std::vector<VkExtensionProperties>& extensionProperties, const std::string_view requestedExtension) {
    for (const VkExtensionProperties& extension : extensionProperties) {
      if (requestedExtension == extension.extensionName)
        return true;
    }
    return false;
  }

  VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* userData)
  {
    switch (severity) {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        DEBUG_VERBOSE("[VK] %s", data->pMessage);
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        DEBUG_INFO("[VK] %s", data->pMessage);
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        DEBUG_WARNING("[VK] %s", data->pMessage);
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        DEBUG_ERROR("[VK] %s", data->pMessage);
        break;
      default:
        break;
    }
    return VK_FALSE;
  }

  // Get the most appropriate queue families for each queue type.
  void getQueueFamilyIndices(
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    uint32_t& outGraphics,
    uint32_t& outPresent,
    uint32_t& outCompute,
    uint32_t& outTransfer,
    std::vector<VkQueueFamilyProperties>& familyProperties)
  {
    outGraphics = UINT32_MAX;
    outPresent = UINT32_MAX;
    outCompute = UINT32_MAX;
    outTransfer = UINT32_MAX;
    uint32_t minTransferScore {UINT32_MAX};

    uint32_t familyCount {0};
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
    familyProperties.resize(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, familyProperties.data());

    for (uint32_t i{0}; i < (uint32_t)familyProperties.size(); ++i) {
      uint32_t curTransferScore {0};
      if (familyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        { outGraphics = i; ++curTransferScore; }
      VkBool32 presentSupport {false};
      vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
      if (presentSupport)
        { outPresent = i; ++curTransferScore; }
      if (familyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        { outCompute = i; ++curTransferScore; }
      if ((familyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && (curTransferScore < minTransferScore))
        { outTransfer = i; minTransferScore = curTransferScore; }
    }
  }

  bool buildSwapchain() {
    using namespace hlgl;
    auto timeStart = std::chrono::high_resolution_clock::now();

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_s, surface_s, &caps);

    // Get the number of images that the swapchain should contain.  We want at least 2.
    uint32_t imgCount = (caps.maxImageCount > caps.minImageCount) ?
      std::clamp<uint32_t>(2, caps.minImageCount, caps.maxImageCount) :
      std::max<uint32_t>(2, caps.minImageCount);

    // Select the most appropriate surface format based on whether HDR is enabled.
    // The initial format used here (8-bit BGRA and nonlinear SRGB) is the only combination guaranteed to be available by the Vulkan spec, and is appropriate for "HDR off".
    VkSurfaceFormatKHR surfaceFormat {.format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR};
    if (hdr_s) {
      uint32_t formatCount {0};
      vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_s, surface_s, &formatCount, nullptr);
      std::vector<VkSurfaceFormatKHR> formats(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_s, surface_s, &formatCount, formats.data());
      for (const VkSurfaceFormatKHR& format : formats) {
        if ((format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
             format.format == VK_FORMAT_R16G16B16_SFLOAT) && (
             format.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT ||
             format.colorSpace == VK_COLOR_SPACE_HDR10_HLG_EXT ||
             format.colorSpace == VK_COLOR_SPACE_DOLBYVISION_EXT))
          surfaceFormat = format;
      }
      if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB) {
        DEBUG_WARNING("HDR was requested but an appropriate HDR surface format could not be found.");
        hdr_s = false;
      }
    }
    swapchainFormat_s = surfaceFormat.format;

    // Get the swapchain extent.
    if (caps.currentExtent.width != UINT32_MAX && caps.currentExtent.height != UINT32_MAX)
      swapchainExtent_s = caps.currentExtent;
    else {
      swapchainExtent_s.width = std::clamp<uint32_t>(displayWidth_s, caps.minImageExtent.width, caps.maxImageExtent.width);
      swapchainExtent_s.height = std::clamp<uint32_t>(displayHeight_s, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // Get the sharing mode and resolve queue family indices.
    VkSharingMode sharing;
    std::vector<uint32_t> queueFamilyIndices {};
    if (graphicsQueueFamily_s == presentQueueFamily_s)
      sharing = VK_SHARING_MODE_EXCLUSIVE;
    else {
      sharing = VK_SHARING_MODE_CONCURRENT;
      queueFamilyIndices = {graphicsQueueFamily_s, presentQueueFamily_s};
    }

    // Get the present mode.
    swapchainPresentMode_s = translate(vsync_s);

    // Only FIFO is guaranteed to be available, so check for present mode support if we want anything else.
    if (swapchainPresentMode_s != VK_PRESENT_MODE_FIFO_KHR) {
      uint32_t presentModeCount {0};
      vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_s, surface_s, &presentModeCount, nullptr);
      std::vector<VkPresentModeKHR> presentModes(presentModeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_s, surface_s, &presentModeCount, presentModes.data());
      bool found {false};
      for (VkPresentModeKHR mode : presentModes) {
        if (mode == swapchainPresentMode_s) {
          found = true;
          break;
        }
      }
      if (!found) {
        DEBUG_WARNING("Desired Vsync mode '%s' is unavailable, using FIFO instead.", enumToStr(vsync_s));
        swapchainPresentMode_s = VK_PRESENT_MODE_FIFO_KHR;
        vsync_s = VsyncMode::Fifo;
      }
    }

    VkSwapchainKHR newSwapchain;
    VkSwapchainCreateInfoKHR ci {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface_s,
      .minImageCount = imgCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = swapchainExtent_s,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .imageSharingMode = sharing,
      .queueFamilyIndexCount = (uint32_t)queueFamilyIndices.size(),
      .pQueueFamilyIndices = queueFamilyIndices.data(),
      .preTransform = caps.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = swapchainPresentMode_s,
      .clipped = true,
      .oldSwapchain = swapchain_s };
    if (!VKCHECK(vkCreateSwapchainKHR(device_s, &ci, nullptr, &newSwapchain)))
      return false;

    // Destroy the old swapchain.
    swapchainImages_s.clear();
    if (swapchain_s)
      vkDestroySwapchainKHR(device_s, swapchain_s, nullptr);

    // Save the new swapchain.
    swapchain_s = newSwapchain;
    if (swapchain_s == nullptr) {
      DEBUG_ERROR("Failed to create swapchain.");
      return false;
    }

    if (gpu_s.enabledFeatures & Feature::Validation) {
      VkDebugUtilsObjectNameInfoEXT info {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
        .objectHandle = (uint64_t)swapchain_s,
        .pObjectName = "swapchain_s" };
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        return false;
    }

    displayWidth_s = swapchainExtent_s.width;
    displayHeight_s = swapchainExtent_s.height;

    // Get the swapchain images.
    uint32_t imageCount {0};
    vkGetSwapchainImagesKHR(device_s, swapchain_s, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(device_s, swapchain_s, &imageCount, images.data());

    // Create a submit semaphore for each swapchain image.
    if (images.size() != submitSemaphores_s.size()) {
      if (submitSemaphores_s.size() > 0) {
        VkSemaphoreWaitInfo swi {
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
          .semaphoreCount = static_cast<uint32_t>(submitSemaphores_s.size()),
          .pSemaphores = submitSemaphores_s.data()};
        if (vkWaitSemaphores(device_s, &swi, 1000000000) == VK_TIMEOUT) {
          DEBUG_ERROR("Timeout waiting for submit semaphores during swapchain rebuild.");
          return false;
        }
        for (VkSemaphore sem : submitSemaphores_s) {
          if (sem)
            vkDestroySemaphore(device_s, sem, nullptr);
        }
        submitSemaphores_s.clear();
      }
      submitSemaphores_s.resize(images.size());
      VkSemaphoreCreateInfo sci {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
      for (VkSemaphore& sem : submitSemaphores_s) {
        vkCreateSemaphore(device_s, &sci, nullptr, &sem);
      }
    }

    // Use the images to create textures, which handle image views and such.
    swapchainImages_s.reserve(images.size());
    for (size_t i {0}; i < images.size(); ++i) {
      char debugName[256]; snprintf(debugName, 256, "swapchainTextures_s[%u]", 1);
      swapchainImages_s.emplace_back(Texture::CreateParams{
        .width = swapchainExtent_s.width,
        .height = swapchainExtent_s.height,
        .format = translate(surfaceFormat.format),
        .extraData = images[i],
        .debugName = debugName
      });
      
    }
    
    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    DEBUG_VERBOSE("Created swapchain (%u x %u) (%s - %s - %s) (took %.2fms)",
      displayWidth_s, displayHeight_s,
      string_VkFormat(surfaceFormat.format), string_VkColorSpaceKHR(surfaceFormat.colorSpace), string_VkPresentModeKHR(swapchainPresentMode_s),
      (double)timeElapsed.count() / 1000.0);
    return true;
  }

  uint64_t alignedSize(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
  }

} // namespace <anon>

void hlgl::debugPrint(hlgl::DebugSeverity severity, const char* fmt, ...) {
  if (debugCallback_s) {
    static constexpr size_t bufferSize {1024*16};
    static std::vector<char> buffer {};
    if (buffer.size() == 0)
      buffer.resize(bufferSize);
    
    va_list args;
    va_start(args,fmt);
    vsnprintf(buffer.data(), bufferSize, fmt, args);
    va_end(args);

    debugCallback_s(severity, buffer.data());
  }
}

bool hlgl::initContext(InitContextParams params) {
  auto totalTimeStart = std::chrono::high_resolution_clock::now();

  // Save the debug callback so the user can be notified of any errors.
  debugCallback_s = params.debugCallback;

  // Make sure we aren't trying to initialize more than once.
  if (initSuccess_s) {
    DEBUG_FATAL("HLGL context cannot be initialized more than once.");
    return false;
  }

  // Features which are required are also preferred, don't make the user repeat themselves.
  params.preferredFeatures |= params.requiredFeatures;

  // Get the window dimensions.
  #if defined HLGL_WINDOW_LIBRARY_GLFW
  {
    int32_t w {0}, h {0};
    glfwGetWindowSize(params.window, &w, &h);
    displayWidth_s = static_cast<uint32_t>(w);
    displayHeight_s = static_cast<uint32_t>(h);
  }
  #elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
  {
    RECT clientRect {}; 
    GetClientRect(params.window, &clientRect);
    displayWidth_s = static_cast<uint32_t>(std::max<int32_t>(0, clientRect.right - clientRect.left));
    displayHeight_s = static_cast<uint32_t>(std::max<int32_t>(0, clientRect.bottom - clientRect.top));
  }
  #endif

  vsync_s = params.vsync;
  hdr_s = params.hdr;

  /////////////////////////////////////////////////////////////////////////////
  // Create Instance
  {
    auto timeStart = std::chrono::high_resolution_clock::now();

    // Initialize Volk to fetch extension function pointers and such.
    if (!VKCHECK(volkInitialize())) {
      DEBUG_FATAL("Failed to initialize volk; no vulkan-capable drivers installed?");
      return false;
    }

    VkApplicationInfo appInfo {
      .pApplicationName = params.appName ? "" : params.appName,
      .applicationVersion = VK_MAKE_VERSION(params.appVer.major, params.appVer.minor, params.appVer.patch),
      .pEngineName = params.engineName ? "" : params.engineName,
      .engineVersion = VK_MAKE_VERSION(params.engineVer.major, params.engineVer.minor, params.engineVer.patch),
      .apiVersion = VK_API_VERSION_1_4 };

    ////////// Handle Layers //////////

    constexpr const char* VK_LAYER_KHRONOS_validation = "VK_LAYER_KHRONOS_validation";
    constexpr size_t maxLayers_c {1};
    hlgl::Array<const char*, maxLayers_c> requiredLayers;
    hlgl::Array<const char*, maxLayers_c> optionalLayers;

    if (params.requiredFeatures & Feature::Validation)
      requiredLayers.push_back(VK_LAYER_KHRONOS_validation);
    else
      optionalLayers.push_back(VK_LAYER_KHRONOS_validation);

    // Get the list of all Vulkan layers supported by the driver.
    uint32_t layerCount {0};
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layerProperties(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

    // Find required layers.
    hlgl::Array<const char*, maxLayers_c> reqLayersFound;
    hlgl::Array<const char*, maxLayers_c> reqLayersMissing;
    for (const char* layer : requiredLayers) {
      if (isLayerSupported(layerProperties, layer))
        reqLayersFound.push_back(layer);
      else
        reqLayersMissing.push_back(layer);
    }
    
    if (requiredLayers.size() > 0) {
      DEBUG_VERBOSE("Found %zu/%zu required Vulkan layer(s):", reqLayersFound.size(), requiredLayers.size());
      for (auto layer : reqLayersFound) { DEBUG_VERBOSE("  - %s", layer); }
      if (reqLayersMissing.size() > 0) {
        DEBUG_FATAL("Missing required Vulkan layer(s):");
        for (auto layer : reqLayersMissing) { DEBUG_FATAL("  - %s", layer); }
        return false;
      }
    }

    // Find optional layers.
    hlgl::Array<const char*, maxLayers_c> optLayersFound {};
    hlgl::Array<const char*, maxLayers_c> optLayersMissing {};
    for (const char* layer : optionalLayers) {
      if (isLayerSupported(layerProperties, layer))
        optLayersFound.push_back(layer);
      else
        optLayersMissing.push_back(layer);
    }

    if (optionalLayers.size() > 0) {
      DEBUG_VERBOSE("Found %zu/%zu optional Vulkan layer(s):", optLayersFound.size(), optionalLayers.size());
      for (auto layer : optLayersFound) { DEBUG_VERBOSE("  - %s", layer); }
      if (optLayersMissing.size() > 0) {
        DEBUG_VERBOSE("Missing optional Vulkan layer(s):");
        for (auto layer : optLayersMissing) { DEBUG_VERBOSE("  - %s", layer); }
      }
    }

    ////////// Handle Extensions //////////

    constexpr size_t maxExtensions_c {8};
    hlgl::Array<const char*, maxExtensions_c> requiredExtensions;
    hlgl::Array<const char*, maxExtensions_c> optionalExtensions;

    // Start with required platform-specific extensions.
    #if defined HLGL_WINDOW_LIBRARY_GLFW
    {
      uint32_t glfwExtCount {0};
      const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtCount);
      for (uint32_t i {0}; i < glfwExtCount; ++i) {
        requiredExtensions.push_back(glfwExtensions[i]);
      }
    }
    #elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
      requiredExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
      requiredExtensions.push_back("VK_KHR_win32_surface");
    #endif

    // Color space extension is always optional, since HDR should be toggleable at runtime.
    optionalExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);

    // Validation may or may not be required depending on user preference.
    if ((params.requiredFeatures & Feature::Validation))
      requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    else
      optionalExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Get the list of all instance extensions supported by the driver.
    uint32_t extensionCount {0};
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());

    // Find required extensions.
    hlgl::Array<const char*, maxExtensions_c> reqExtensionsFound {};
    hlgl::Array<const char*, maxExtensions_c> reqExtensionsMissing {};
    for (const char* extension : requiredExtensions) {
      if (isExtensionSupported(extensionProperties, extension))
        reqExtensionsFound.push_back(extension);
      else
        reqExtensionsMissing.push_back(extension);
    }

    DEBUG_VERBOSE("Found %zu/%zu required Vulkan instance extension(s):", reqExtensionsFound.size(), requiredExtensions.size());
    for (const char* extension : reqExtensionsFound) { DEBUG_VERBOSE("  - %s", extension); }
    if (reqExtensionsMissing.size() > 0) {
      DEBUG_FATAL("Missing required Vulkan instance extension(s):");
      for (const char* extension : reqExtensionsMissing) { DEBUG_FATAL("  - %s", extension); }
      return false;
    }

    // Find optional extensions.
    hlgl::Array<const char*, maxExtensions_c> optExtensionsFound {};
    hlgl::Array<const char*, maxExtensions_c> optExtensionsMissing {};
    for (const char* extension : optionalExtensions) {
      if (isExtensionSupported(extensionProperties, extension))
        optExtensionsFound.push_back(extension);
      else
        optExtensionsMissing.push_back(extension);
    }

    DEBUG_VERBOSE("Found %zu/%zu optional Vulkan instance extension(s):", optExtensionsFound.size(), optionalExtensions.size());
    for (const char* extension : optExtensionsFound) { DEBUG_VERBOSE("  - %s", extension); }
    if (optExtensionsMissing.size() > 0) {
      DEBUG_VERBOSE("Missing optional Vulkan instance extension(s):");
      for (const char* extension : optExtensionsMissing) { DEBUG_VERBOSE("  - %s", extension); }
    }

    // If the user didn't request validation, remove the layer and extension from the optional sets.

    size_t validationLayerIndex = reqLayersFound.findStr(VK_LAYER_KHRONOS_validation);
    if (validationLayerIndex == SIZE_MAX)
      validationLayerIndex = optLayersFound.findStr(VK_LAYER_KHRONOS_validation);
    
    size_t debugExtensionIndex = reqExtensionsFound.findStr(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (debugExtensionIndex == SIZE_MAX)
      debugExtensionIndex = optExtensionsFound.findStr(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (validationLayerIndex != SIZE_MAX && debugExtensionIndex != SIZE_MAX)
      gpu_s.supportedFeatures |= Feature::Validation;
    
    if (!(params.preferredFeatures & Feature::Validation)) {
      if (validationLayerIndex != SIZE_MAX)
        optLayersFound.erase(validationLayerIndex);
      if (debugExtensionIndex != SIZE_MAX)
        optExtensionsFound.erase(debugExtensionIndex);
    }
    // If the user DID request validation, AND it's supported, flag it as enabled.
    else if (gpu_s.supportedFeatures & Feature::Validation) {
      gpu_s.enabledFeatures |= Feature::Validation;
    }

    // Finally, instance creation.

    // Combine the sets of found layers/extensions into a single list which we'll send to instance creation.
    reqLayersFound.merge(optLayersFound);
    reqExtensionsFound.merge(optExtensionsFound);

    VkInstanceCreateInfo ci {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .flags = 0,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(reqLayersFound.size()),
      .ppEnabledLayerNames = reqLayersFound.data(),
      .enabledExtensionCount = static_cast<uint32_t>(reqExtensionsFound.size()),
      .ppEnabledExtensionNames = reqExtensionsFound.data()
    };

    // If validation is enabled, add the feature to the pnext chain.
    VkValidationFeatureEnableEXT enabledValidationFeatures[] = {
      VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT };
    VkValidationFeaturesEXT vf {
      .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
      .enabledValidationFeatureCount = 1,
      .pEnabledValidationFeatures = enabledValidationFeatures };
    if (gpu_s.enabledFeatures & Feature::Validation) {
      ci.pNext = &vf;
    }

    if (!VKCHECK(vkCreateInstance(&ci, nullptr, &instance_s)) || !instance_s) {
      debugPrint(DebugSeverity::Fatal, "Failed to create Vulkan instance.");
      return false;
    }

    volkLoadInstance(instance_s);
    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    debugPrint(DebugSeverity::Verbose, "Created Vulkan instance (took %.2fms)", (double)timeElapsed.count() / 1000.0);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Debugging
  if (gpu_s.enabledFeatures & Feature::Validation) {
    auto timeStart = std::chrono::high_resolution_clock::now();
    VkDebugUtilsMessengerCreateInfoEXT ci = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType =     VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = vulkanDebugCallback };

    if (!VKCHECK(vkCreateDebugUtilsMessengerEXT(instance_s, &ci, nullptr, &debug_s)) || !debug_s) {
      DEBUG_WARNING("Failed to create Vulkan debug messenger.");
      gpu_s.enabledFeatures ^= (~Feature::Validation);
    }

    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    DEBUG_VERBOSE("Created Vulkan debug messenger (took %.2fms)", (double)timeElapsed.count() / 1000.0);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Create Surface
  {
    auto timeStart = std::chrono::high_resolution_clock::now();
    #if defined HLGL_WINDOW_LIBRARY_GLFW
    if (!VKCHECK(glfwCreateWindowSurface(instance_s, params.window, nullptr, &surface_s)) || !surface_s) {
      DEBUG_FATAL("Failed to create Vulkan window surface for GLFW.");
      return false;
    }
    #elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
    {
      VkWin32SurfaceCreateInfoKHR ci {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandle(nullptr),
        .hwnd = params.window };
      if (!VKCHECK(vkCreateWin32SurfaceKHR(instance_s, &ci, nullptr, &surface_s)) || !surface_s) {
        DEBUG_FATAL("Failed to create Vulkan window surface for Win32.");
        return false;
      }
    }
    #endif

    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    DEBUG_VERBOSE("Created Vulkan window surface (took %.2fms)", (double)timeElapsed.count() / 1000.0);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Device Extensions
  auto deviceTimeStart = std::chrono::high_resolution_clock::now();
  constexpr size_t maxDeviceExtensions_c {32};
  hlgl::Array<const char*, maxDeviceExtensions_c> requiredDeviceExtensions;
  hlgl::Array<const char*, maxDeviceExtensions_c> optionalDeviceExtensions;
  std::vector<VkExtensionProperties> extensionProperties;
  std::vector<VkQueueFamilyProperties> queueFamilyProperties;
  {
    requiredDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    
    if (params.requiredFeatures & Feature::MeshShading)
      requiredDeviceExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    else
      optionalDeviceExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    
    if (params.requiredFeatures & Feature::RayTracing)
      requiredDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    else
      optionalDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Select Physical Device
  {
    uint32_t availableDevicesCount {0};
    vkEnumeratePhysicalDevices(instance_s, &availableDevicesCount, nullptr);
    std::vector<VkPhysicalDevice> availableDevices(availableDevicesCount);
    vkEnumeratePhysicalDevices(instance_s, &availableDevicesCount, availableDevices.data());

    // Keep track of a list of appropriate physical devices along with their properties.
    using GpuPair = std::pair<GpuProperties, VkPhysicalDevice>;
    std::vector<GpuPair> appropriateDevices;
    appropriateDevices.reserve(availableDevicesCount);

    for (VkPhysicalDevice physicalDevice : availableDevices) {
      VkPhysicalDeviceProperties pdProperties {};
      vkGetPhysicalDeviceProperties(physicalDevice, &pdProperties);
      DEBUG_VERBOSE("Found physical device '%s', checking properties...",
        pdProperties.deviceName);

      // Make sure the neccessary queue families are supported.
      uint32_t graphicsFamily, presentFamily, computeFamily, transferFamily;
      getQueueFamilyIndices(physicalDevice, surface_s, graphicsFamily, presentFamily, computeFamily, transferFamily, queueFamilyProperties);
      if (graphicsFamily == UINT32_MAX ||
          presentFamily == UINT32_MAX ||
          computeFamily == UINT32_MAX ||
          transferFamily == UINT32_MAX)
      {
        DEBUG_VERBOSE("  ...required queue familes not supported, skipping.");
        continue;
      }

      // Get the list of device extensions supported by this physical device.
      uint32_t extensionPropertiesCount {0};
      vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionPropertiesCount, nullptr);
      extensionProperties.resize(extensionPropertiesCount);
      vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionPropertiesCount, extensionProperties.data());

      // Make sure the neccessary device extensions are supported.
      bool skip {false};
      hlgl::Array<const char*, maxDeviceExtensions_c> supportedExtensions;
      for (const char* extension : requiredDeviceExtensions) {
        if (!isExtensionSupported(extensionProperties, extension)) {
          DEBUG_VERBOSE("  ...required device extension '%s' not supported, skipping.", extension);
          skip = true;
        }
        else {
          supportedExtensions.push_back(extension);
        }
      }
      if (skip) continue;

      for (const char* extension : optionalDeviceExtensions) {
        if (isExtensionSupported(extensionProperties, extension))
          supportedExtensions.push_back(extension);
      }

      // Assemble the GpuProperties struct.
      GpuProperties properties {
        .name = pdProperties.deviceName,
        .apiVerMajor = VK_VERSION_MAJOR(pdProperties.apiVersion),
        .apiVerMinor = VK_VERSION_MINOR(pdProperties.apiVersion),
        .apiVerPatch = VK_VERSION_PATCH(pdProperties.apiVersion),
        .driverVerMajor = VK_VERSION_MAJOR(pdProperties.driverVersion),
        .driverVerMinor = VK_VERSION_MINOR(pdProperties.driverVersion),
        .driverVerPatch = VK_VERSION_PATCH(pdProperties.driverVersion),
        .supportedFeatures = gpu_s.supportedFeatures,
        .enabledFeatures = gpu_s.enabledFeatures };
      
      // Record the device memory.
      VkPhysicalDeviceMemoryProperties memory;
      vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memory);
      for (uint32_t i {0}; i < memory.memoryHeapCount; ++i) {
        if (memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
          properties.deviceMemory = std::max(properties.deviceMemory, static_cast<uint64_t>(memory.memoryHeaps[i].size));
      }

      // Record the device type.
      switch (pdProperties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_CPU:             properties.type = GpuType::Cpu; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:     properties.type = GpuType::Virtual; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:  properties.type = GpuType::Integrated; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:    properties.type = GpuType::Discrete; break;
        default: properties.type = GpuType::Other; break;
      }

      // Record the Vendor ID.
      switch (pdProperties.vendorID) {
        case 0x1002: properties.vendor = GpuVendor::AMD; break;
        case 0x1010: properties.vendor = GpuVendor::ImgTec; break;
        case 0x10de: properties.vendor = GpuVendor::NVIDIA; break;
        case 0x1385: properties.vendor = GpuVendor::ARM; break;
        case 0x5143: properties.vendor = GpuVendor::Qualcomm; break;
        case 0x8086: properties.vendor = GpuVendor::INTEL; break;
        default: properties.vendor = GpuVendor::Other; break;
      }
      
      if (supportedExtensions.findStr(VK_EXT_MESH_SHADER_EXTENSION_NAME) != SIZE_MAX)
        properties.supportedFeatures |= Feature::MeshShading;
      
      if (supportedExtensions.findStr(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) != SIZE_MAX)
        properties.supportedFeatures |= Feature::RayTracing;
      
      // Make sure there's at least one supported surface format.
      uint32_t surfaceFormatsCount {0};
      vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface_s, &surfaceFormatsCount, nullptr);
      if (surfaceFormatsCount == 0) {
        DEBUG_VERBOSE("  ...no surface formats available, skipping.");
        continue;
      }

      // Make sure there's at least one supported present mode.
      uint32_t presentModesCount {0};
      vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface_s, &presentModesCount, nullptr);
      if (presentModesCount == 0) {
        DEBUG_VERBOSE("  ...no surface present modes available, skipping.");
        continue;
      }

      // TODO: Check for feature support.

      // Pack it up and add it to the list.
      GpuPair pair {properties, physicalDevice};
      appropriateDevices.push_back(pair);
    }

    if (appropriateDevices.empty()) {
      DEBUG_FATAL("Failed to find an appropriate GPU.");
      return false;
    }

    // If the user requested a particular gpu, look for it here.
    GpuPair chosenDevice = {};
    if (params.preferredGpu) {
      for (const GpuPair& pair : appropriateDevices) {
        if (pair.first.name == std::string_view(params.preferredGpu)) {
          chosenDevice = pair;
          break;
        }
      }
      if (chosenDevice.second == nullptr)
        DEBUG_WARNING("Couldn't find preferred GPU '%s', will choose the most appropriate device instead.", params.preferredGpu);
    }

    // If we didn't find a preferred physical device (or the user didn't request one), choose the most appropiate device.
    if (chosenDevice.second == nullptr) {
      std::sort(appropriateDevices.begin(), appropriateDevices.end(), [&](const GpuPair& a, const GpuPair& b)
      {
        // First priority is support for preferred features.
        //int aPreferredFeatureCount = a.first.supportedFeatures.bitsInCommon(preferredFeatures);
        //int bPreferredFeatureCount = b.first.supportedFeatures.bitsInCommon(preferredFeatures);
        //if (aPreferredFeatureCount != bPreferredFeatureCount)
        //  return (aPreferredFeatureCount < bPreferredFeatureCount);

        // If the supported feature count is the same, then compare the device type.  Discrete GPU is ideal.
        if (a.first.type != b.first.type)
          return ((int)a.first.type < (int)b.first.type);

        // If the device type is also the same, compare the amount of available device memory.
        return (a.first.deviceMemory < b.first.deviceMemory);
      });
      chosenDevice = appropriateDevices.back();
    }

    gpu_s = chosenDevice.first;
    physicalDevice_s = chosenDevice.second;

    DEBUG_INFO("Using %s (%s) with %zu bytes of device memory.",
      gpu_s.name.c_str(), enumToStr(gpu_s.type), gpu_s.deviceMemory);
    DEBUG_INFO("Vulkan API version %u.%u.%u, Driver version %u.%u.%u",
      gpu_s.apiVerMajor, gpu_s.apiVerMinor, gpu_s.apiVerPatch,
      gpu_s.driverVerMajor, gpu_s.driverVerMinor, gpu_s.driverVerPatch);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Logical Device
  {
    DEBUG_VERBOSE("Found %zu required Vulkan device extension(s):", requiredDeviceExtensions.size());
    for (const char* extension : requiredDeviceExtensions) {
      DEBUG_VERBOSE("  - %s", extension);
    }

    // Get the list of optional device extensions which are supported.
    uint32_t extensionPropertiesCount {0};
    vkEnumerateDeviceExtensionProperties(physicalDevice_s, nullptr, &extensionPropertiesCount, nullptr);
    extensionProperties.resize(extensionPropertiesCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice_s, nullptr, &extensionPropertiesCount, extensionProperties.data());

    hlgl::Array<const char*, maxDeviceExtensions_c> supportedOptionalExtensions;
    for (const char* extension : optionalDeviceExtensions) {
      if (isExtensionSupported(extensionProperties, extension))
        supportedOptionalExtensions.push_back(extension);
    }

    DEBUG_VERBOSE("Found %zu/%zu optional Vulkan device extension(s):",
      supportedOptionalExtensions.size(), optionalDeviceExtensions.size());
    for (const char* extension : supportedOptionalExtensions) {
      DEBUG_VERBOSE("  - %s", extension);
    }

    if ((gpu_s.supportedFeatures & Feature::MeshShading) &&
        (params.preferredFeatures & Feature::MeshShading))
    {
      requiredDeviceExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
      gpu_s.enabledFeatures |= Feature::MeshShading;
    }

    if ((gpu_s.supportedFeatures & Feature::RayTracing) && 
        (params.preferredFeatures & Feature::RayTracing))
    {
      requiredDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
      gpu_s.enabledFeatures |= Feature::RayTracing;
    }

    // Assemble queue family indices.
    getQueueFamilyIndices(physicalDevice_s, surface_s,
      graphicsQueueFamily_s, presentQueueFamily_s, computeQueueFamily_s, transferQueueFamily_s, queueFamilyProperties);
    hlgl::Array<uint32_t, 4> uniqueIndices;
    uniqueIndices.push_back(graphicsQueueFamily_s);
    size_t found;
    found = SIZE_MAX; found = uniqueIndices.findItem(presentQueueFamily_s); if (found == SIZE_MAX) {uniqueIndices.push_back(presentQueueFamily_s);}
    found = SIZE_MAX; found = uniqueIndices.findItem(computeQueueFamily_s); if (found == SIZE_MAX) {uniqueIndices.push_back(computeQueueFamily_s);}
    found = SIZE_MAX; found = uniqueIndices.findItem(transferQueueFamily_s); if (found == SIZE_MAX) {uniqueIndices.push_back(transferQueueFamily_s);}

    // Build the queue create infos.
    constexpr float queuePriority_c {1.0f};
    hlgl::Array<VkDeviceQueueCreateInfo, 4> qci;
    for (uint32_t index : uniqueIndices) {
      VkDeviceQueueCreateInfo ci {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = index,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority_c };
      qci.push_back(ci);
    }

    // Enable desired features.
    // TODO: Test for the availability of these during physical device selection.
    void* pNext {nullptr};

    VkPhysicalDeviceMeshShaderFeaturesEXT msf {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
      .pNext = pNext,
      .taskShader = true,
      .meshShader = true };
    if (gpu_s .enabledFeatures & Feature::MeshShading)
      pNext = &msf;

    VkPhysicalDeviceVulkan13Features df13 {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = pNext,
      .synchronization2 = true,
      .dynamicRendering = true };
    pNext = &df13;

    VkPhysicalDeviceVulkan12Features df12 {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = pNext,
      .drawIndirectCount = true,
      .storageBuffer8BitAccess = true,
      .uniformAndStorageBuffer8BitAccess = true,
      .storagePushConstant8 = true,
      .shaderFloat16 = true,
      .shaderInt8 = true,
      .descriptorIndexing = true,
      .shaderSampledImageArrayNonUniformIndexing = true,
      .shaderStorageImageArrayNonUniformIndexing = true,
      .descriptorBindingSampledImageUpdateAfterBind = true,
      .descriptorBindingStorageImageUpdateAfterBind = true,
      .descriptorBindingUpdateUnusedWhilePending = true,
      .descriptorBindingPartiallyBound = true,
      .descriptorBindingVariableDescriptorCount = true,
      .runtimeDescriptorArray = true,
      .samplerFilterMinmax = true,
      .bufferDeviceAddress = true };
    pNext = &df12;

    VkPhysicalDeviceVulkan11Features df11 {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .pNext = pNext,
      .storageBuffer16BitAccess = true,
      .shaderDrawParameters = true };
    pNext = &df11;

    VkPhysicalDeviceFeatures df10 {
      .samplerAnisotropy = true,
      .pipelineStatisticsQuery = true,
      .shaderInt16 = true };

    VkDeviceCreateInfo ci {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = pNext,
      .queueCreateInfoCount = static_cast<uint32_t>(qci.size()),
      .pQueueCreateInfos = qci.data(),
      .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
      .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
      .pEnabledFeatures = &df10 };

    // Finally create the logical device.
    if (!VKCHECK(vkCreateDevice(physicalDevice_s, &ci, nullptr, &device_s)) || !device_s) {
      DEBUG_FATAL("Failed to create Vulkan logical device.");
      return false;
    }

    // Get device function pointers to bypass Vulkan's loader dispatch.
    volkLoadDevice(device_s);

    // Now that we have a Device, we can go back and add debug names for existing objects.
    if (gpu_s.enabledFeatures & Feature::Validation) {
      VkDebugUtilsObjectNameInfoEXT info {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
      
      info.objectType = VK_OBJECT_TYPE_INSTANCE;
      info.objectHandle = (uint64_t)instance_s;
      info.pObjectName = "instance_s";
      if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        DEBUG_WARNING("Failed to set Vulkan debug name for 'instance_s'.");
      
      info.objectType = VK_OBJECT_TYPE_SURFACE_KHR;
      info.objectHandle = (uint64_t)surface_s;
      info.pObjectName = "surface_s";
      if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        DEBUG_WARNING("Failed to set Vulkan debug name for 'surface_s'.");
      
      info.objectType = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
      info.objectHandle = (uint64_t)physicalDevice_s;
      info.pObjectName = "physicalDevice_s";
      if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        DEBUG_WARNING("Failed to set Vulkan debug name for 'physicalDevice_s'.");

      info.objectType = VK_OBJECT_TYPE_DEVICE;
      info.objectHandle = (uint64_t)device_s;
      info.pObjectName = "device_s";
      if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        DEBUG_WARNING("Failed to set Vulkan debug name for 'device_s'.");
    }

    auto deviceTimeEnd = std::chrono::high_resolution_clock::now();
    auto deviceTimeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(deviceTimeEnd - deviceTimeStart);
    DEBUG_VERBOSE("Created Vulkan logical device (took %.2f)", (double)deviceTimeElapsed.count() / 1000.0);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Queues
  {
    auto timeStart = std::chrono::high_resolution_clock::now();
    vkGetDeviceQueue(device_s, graphicsQueueFamily_s, 0, &graphicsQueue_s);
    vkGetDeviceQueue(device_s, presentQueueFamily_s, 0, &presentQueue_s);
    vkGetDeviceQueue(device_s, computeQueueFamily_s, 0, &computeQueue_s);
    vkGetDeviceQueue(device_s, transferQueueFamily_s, 0, &transferQueue_s);

    if (gpu_s.enabledFeatures & Feature::Validation) {
      std::set<VkQueue> queueSet {graphicsQueue_s, presentQueue_s, computeQueue_s, transferQueue_s};
      for (VkQueue queue : queueSet) {
        std::string queueName = "queues.";
        if (queue == graphicsQueue_s && queue == presentQueue_s && queue == computeQueue_s && queue == transferQueue_s)
          queueName += "all";
        else {
          bool isFirst {true};
          if (queue == graphicsQueue_s) { queueName += "graphics"; isFirst = false; }
          if (queue == presentQueue_s) { if (isFirst) queueName += "present"; else queueName += "|present"; isFirst = false; }
          if (queue == computeQueue_s) { if (isFirst) queueName += "compute"; else queueName += "|compute"; isFirst = false; }
          if (queue == transferQueue_s) { if (isFirst) queueName += "transfer"; else queueName += "|transfer"; isFirst = false; }
        }

        VkDebugUtilsObjectNameInfoEXT info {
          .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
          .objectType = VK_OBJECT_TYPE_QUEUE,
          .objectHandle = (uint64_t)queue,
          .pObjectName = queueName.c_str() };
        if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
          DEBUG_WARNING("Failed to set Vulkan debug name for '%s'.", queueName.c_str());
      }
    }

    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    DEBUG_VERBOSE("Using Vulkan device queues with family indices: %u(graphics), %u(present), %u(compute), %u(transfer) (took %.2fms)",
      graphicsQueueFamily_s, presentQueueFamily_s, computeQueueFamily_s, transferQueueFamily_s,
      (double)timeElapsed.count() / 1000.0);
  }
  
  /////////////////////////////////////////////////////////////////////////////
  // Initialize Vulkan Memory Allocator
  {
    auto timeStart = std::chrono::high_resolution_clock::now();
    VmaVulkanFunctions volkFunctions {
      vkGetInstanceProcAddr,
      vkGetDeviceProcAddr,
      vkGetPhysicalDeviceProperties,
      vkGetPhysicalDeviceMemoryProperties,
      vkAllocateMemory,
      vkFreeMemory,
      vkMapMemory,
      vkUnmapMemory,
      vkFlushMappedMemoryRanges,
      vkInvalidateMappedMemoryRanges,
      vkBindBufferMemory,
      vkBindImageMemory,
      vkGetBufferMemoryRequirements,
      vkGetImageMemoryRequirements,
      vkCreateBuffer,
      vkDestroyBuffer,
      vkCreateImage,
      vkDestroyImage,
      vkCmdCopyBuffer,
      vkGetBufferMemoryRequirements2KHR,
      vkGetImageMemoryRequirements2KHR,
      vkBindBufferMemory2KHR,
      vkBindImageMemory2KHR,
      vkGetPhysicalDeviceMemoryProperties2KHR,
      vkGetDeviceBufferMemoryRequirements,
      vkGetDeviceImageMemoryRequirements,
    };

    VmaAllocatorCreateInfo ci {
      .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
      .physicalDevice = physicalDevice_s,
      .device = device_s,
      .pVulkanFunctions = &volkFunctions,
      .instance = instance_s,
    };
    if (!VKCHECK(vmaCreateAllocator(&ci, &allocator_s)) || !allocator_s) {
      DEBUG_FATAL("Failed to create VMA allocator.");
      return false;
    }

    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    DEBUG_VERBOSE("Created VMA allocator (took %.2fms)", (double)timeElapsed.count() / 1000.0);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Command Pool
  {
    auto timeStart = std::chrono::high_resolution_clock::now();
    VkCommandPoolCreateInfo ci {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = graphicsQueueFamily_s };
    
    if (!VKCHECK(vkCreateCommandPool(device_s, &ci, nullptr, &cmdPoolGraphics_s)) || !cmdPoolGraphics_s) {
      DEBUG_FATAL("Failed to create Vulkan graphics command pool.");
      return false;
    }

    ci.queueFamilyIndex = transferQueueFamily_s;
    if (!VKCHECK(vkCreateCommandPool(device_s, &ci, nullptr, &cmdPoolTransfer_s)) || !cmdPoolTransfer_s) {
      DEBUG_FATAL("Failed to create Vulkan transfer command pool.");
      return false;
    }

    if (gpu_s.enabledFeatures & Feature::Validation) {
      VkDebugUtilsObjectNameInfoEXT info {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_COMMAND_POOL,
        .objectHandle = (uint64_t)cmdPoolGraphics_s,
        .pObjectName = "cmdPoolGraphics_s" };
      if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        DEBUG_WARNING("Failed to set Vulkan debug name for 'cmdPoolGraphics_s'.");

      info.objectHandle = (uint64_t)cmdPoolTransfer_s;
      info.pObjectName = "cmdPoolTransfer_s";
      if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        DEBUG_WARNING("Failed to set Vulkan debug name for 'cmdPoolTransfer_s'.");
    }

    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    DEBUG_VERBOSE("Created Vulkan command pools (took %.2fms)", (double)timeElapsed.count() / 1000.0);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Descriptor Layouts and Pool
  {
    auto timeStart = std::chrono::high_resolution_clock::now();
    VkDescriptorBindingFlags descVarFlag[] { 
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
      VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
    
    VkDescriptorSetLayoutBindingFlagsCreateInfo descBindFlags {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
      .bindingCount = 1,
      .pBindingFlags = descVarFlag };

    VkDescriptorSetLayoutBinding descLayoutBindings[] {
      {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,                .descriptorCount = DESCRIPTOR_COUNTS[DESC_TYPE_SAMPLER], .stageFlags = VK_SHADER_STAGE_ALL},
      {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = DESCRIPTOR_COUNTS[DESC_TYPE_COMBINED_IMAGE_SAMPLER], .stageFlags = VK_SHADER_STAGE_ALL},
      {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = DESCRIPTOR_COUNTS[DESC_TYPE_STORAGE_IMAGE], .stageFlags = VK_SHADER_STAGE_ALL},
    };

    VkDescriptorSetLayoutCreateInfo descLayoutInfo {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = &descBindFlags,
      .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
      .bindingCount = 1,
      .pBindings = &descLayoutBindings[0] };
    if (!VKCHECK(vkCreateDescriptorSetLayout(device_s, &descLayoutInfo, nullptr, &descLayouts_s[0])) || !descLayouts_s[0]) {
      DEBUG_FATAL("Failed to create descriptor set layout.");
      return false;
    }
    descLayoutInfo.pBindings = &descLayoutBindings[1];
    if (!VKCHECK(vkCreateDescriptorSetLayout(device_s, &descLayoutInfo, nullptr, &descLayouts_s[1])) || !descLayouts_s[1]) {
      DEBUG_FATAL("Failed to create descriptor set layout.");
      return false;
    }
    descLayoutInfo.pBindings = &descLayoutBindings[2];
    if (!VKCHECK(vkCreateDescriptorSetLayout(device_s, &descLayoutInfo, nullptr, &descLayouts_s[2])) || !descLayouts_s[2]) {
      DEBUG_FATAL("Failed to create descriptor set layout.");
      return false;
    }

    VkDescriptorPoolSize poolSizes[] {
      { .type = VK_DESCRIPTOR_TYPE_SAMPLER,                 .descriptorCount = DESCRIPTOR_COUNTS[DESC_TYPE_SAMPLER] },
      { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  .descriptorCount = DESCRIPTOR_COUNTS[DESC_TYPE_COMBINED_IMAGE_SAMPLER] },
      { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,           .descriptorCount = DESCRIPTOR_COUNTS[DESC_TYPE_STORAGE_IMAGE] }
    };

    VkDescriptorPoolCreateInfo poolInfo {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
      .maxSets = NUM_DESCRIPTOR_SETS,
      .poolSizeCount = NUM_DESCRIPTOR_SETS,
      .pPoolSizes = poolSizes };
    if (!VKCHECK(vkCreateDescriptorPool(device_s, &poolInfo, nullptr, &descPool_s)) || !descPool_s) {
      DEBUG_FATAL("Failed to create descriptor pool.");
      return false;
    }

    VkDescriptorSetVariableDescriptorCountAllocateInfo varCountInfo {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
      .descriptorSetCount = NUM_DESCRIPTOR_SETS,
      .pDescriptorCounts = DESCRIPTOR_COUNTS };
    VkDescriptorSetAllocateInfo descAllocInfo {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = &varCountInfo,
      .descriptorPool = descPool_s,
      .descriptorSetCount = NUM_DESCRIPTOR_SETS,
      .pSetLayouts = descLayouts_s.data()};
    if (!VKCHECK(vkAllocateDescriptorSets(device_s, &descAllocInfo, descSets_s.data()))) {
      DEBUG_FATAL("Failed to allocate descriptor sets.");
      return false;
    }

    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    DEBUG_VERBOSE("Created Vulkan descriptor pool and allocated descriptor sets (took %.2fms)", (double)timeElapsed.count() / 1000.0);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Create the universal pipeline layout
  {
    auto timeStart = std::chrono::high_resolution_clock::now();
    VkPushConstantRange pushConstRange {
      .stageFlags = VK_SHADER_STAGE_ALL,
      .offset = 0,
      .size = 128,  // 128 bytes is the smallest push constant size guaranteed by the Vulkan spec.
    };
    VkPipelineLayoutCreateInfo info {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = (uint32_t)descLayouts_s.size(),
      .pSetLayouts = descLayouts_s.data(),
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pushConstRange
    };
    if (!VKCHECK(vkCreatePipelineLayout(device_s, &info, nullptr, &pipeLayout_s)) || !pipeLayout_s) {
      DEBUG_ERROR("Failed to create pipeline layout.");
      return false;
    }
    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    DEBUG_VERBOSE("Created Vulkan universal pipeline layout (took %.2fms)", (double)timeElapsed.count() / 1000.0);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Swapchain
  if (!buildSwapchain() || !swapchain_s) {
    DEBUG_FATAL("Failed to create swapchain.");
    return false;
  }
  
  /////////////////////////////////////////////////////////////////////////////
  // Initialize Frame Synchronization
  {
    auto timeStart = std::chrono::high_resolution_clock::now();
    VkCommandBufferAllocateInfo ai {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = cmdPoolGraphics_s,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = numFramesInFlight_c };
    if (!VKCHECK(vkAllocateCommandBuffers(device_s, &ai, frameCmdBuffers_s.data())))
      return false;

    for (uint32_t i {0}; i < numFramesInFlight_c; ++i) {
      VkSemaphoreCreateInfo sci {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
      if (!VKCHECK(vkCreateSemaphore(device_s, &sci, nullptr, &acquireSemaphores_s[i])))
        return false;

      VkFenceCreateInfo fci {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT };
      if (!VKCHECK(vkCreateFence(device_s, &fci, nullptr, &frameFences_s[i])))
        return false;

      if (gpu_s.enabledFeatures & Feature::Validation) {
        char debugName[256];

        snprintf(debugName, 256, "frameCmdBuffers_s[%u]", i);
        VkDebugUtilsObjectNameInfoEXT info = {
          .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
        info.objectHandle = (uint64_t)frameCmdBuffers_s[i];
        info.pObjectName = debugName;
        if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
          DEBUG_WARNING("Failed to set Vulkan debug name for '%s'.", debugName);

        snprintf(debugName, 256, "acquireSemaphores_s[%u]", i);
        info.objectType = VK_OBJECT_TYPE_SEMAPHORE;
        info.objectHandle = (uint64_t)acquireSemaphores_s[i];
        info.pObjectName = debugName;
        if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
          DEBUG_WARNING("Failed to set Vulkan debug name for '%s'.", debugName);

        snprintf(debugName, 256, "frameFences_s[%u]", i);
        info.objectType = VK_OBJECT_TYPE_FENCE;
        info.objectHandle = (uint64_t)frameFences_s[i];
        info.pObjectName = debugName;
        if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
          DEBUG_WARNING("Failed to set Vulkan debug name for '%s'.", debugName);
      }
    }

    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    debugPrint(DebugSeverity::Verbose,
      "Created command buffers and synchronization primitives for frames in flight (took %.2fms)",
      (double)timeElapsed.count() / 1000.0);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Create staging buffer for transfers
  {
    auto timeStart = std::chrono::high_resolution_clock::now();

    stagingBuffer_s.emplace(Buffer::CreateParams{
      .usage = BufferUsage::TransferSrc | BufferUsage::HostVisible,
      .size = 1024*1024*100 // Start with a size of 100MB.
    });

    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    debugPrint(DebugSeverity::Verbose,
      "Created staging buffer for transfers (took %.2fms)",
      (double)timeElapsed.count() / 1000.0);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Create default textures
  {
    auto timeStart = std::chrono::high_resolution_clock::now();
    ColorRGBAb a {255, 0, 255, 255};
    ColorRGBAb b {0, 0, 0, 255};
    ColorRGBAb texels[] = {
      a, b, a, b, a, b, a, b,
      b, a, b, a, b, a, b, a,
      a, b, a, b, a, b, a, b,
      b, a, b, a, b, a, b, a,
      a, b, a, b, a, b, a, b,
      b, a, b, a, b, a, b, a,
      a, b, a, b, a, b, a, b,
      b, a, b, a, b, a, b, a,
    };

    defaultTextureNull_s.emplace(Texture::CreateParams{
      .usage = TextureUsage::Storage,
      .width = 8,
      .height = 8,
      .depth = 1,
      .format = ImageFormat::RGBA8i,
      .dataPtr = texels,
      .dataSize = sizeof(texels),
      .debugName = "defaultTextureNull",
      .sampler = Texture::CreateParams::Sampler{
        .wrapping = WrapMode::Repeat,
        .filtering = FilterMode::Nearest, }
    });

    ColorRGBAb white {255,255,255,255};
    defaultTextureWhite_s.emplace(Texture::CreateParams{
      .usage = TextureUsage::Storage,
      .width = 1,
      .height = 1,
      .depth = 1,
      .format = ImageFormat::RGBA8i,
      .dataPtr = &white,
      .dataSize = sizeof(white),
      .debugName = "defaultTextureWhite",
      .sampler = Texture::CreateParams::Sampler{
        .wrapping = WrapMode::ClampToEdge,
        .filtering = FilterMode::Nearest }
    });

    ColorRGBAf gray {0.5f, 0.5f, 0.5f, 1.0f};
    defaultTextureGray_s.emplace(Texture::CreateParams{
      .usage = TextureUsage::Storage,
      .width = 1,
      .height = 1,
      .depth = 1,
      .format = ImageFormat::RGBA32f,
      .dataPtr = &gray,
      .dataSize = sizeof(gray),
      .debugName = "defaultTextureGray",
      .sampler = Texture::CreateParams::Sampler{
        .wrapping = WrapMode::ClampToEdge,
        .filtering = FilterMode::Nearest }
    });

    ColorRGBAb black {0,0,0,255};
    defaultTextureBlack_s.emplace(Texture::CreateParams{
      .usage = TextureUsage::Storage,
      .width = 1,
      .height = 1,
      .depth = 1,
      .format = ImageFormat::RGBA8i,
      .dataPtr = &black,
      .dataSize = sizeof(black),
      .debugName = "defaultTextureBlack",
      .sampler = Texture::CreateParams::Sampler{
        .wrapping = WrapMode::ClampToEdge,
        .filtering = FilterMode::Nearest }
    });
    
    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    DEBUG_VERBOSE("Created default textures (took %.2fms)", (double)timeElapsed.count() / 1000.0);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize ImGui
  {
    auto timeStart = std::chrono::high_resolution_clock::now();
    ImGui::CreateContext();
    #if defined HLGL_WINDOW_LIBRARY_GLFW
    ImGui_ImplGlfw_InitForVulkan(params.window, true);
    #elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
    ImGui_ImplWin32_InitForVulkan(params.window, true);
    #endif
    ImGui_ImplVulkan_InitInfo ii {
      .Instance = instance_s,
      .PhysicalDevice = physicalDevice_s,
      .Device = device_s,
      .QueueFamily = graphicsQueueFamily_s,
      .Queue = graphicsQueue_s,
      .DescriptorPoolSize = 1000,
      .MinImageCount = 3,
      .ImageCount = 3,
      .PipelineInfoMain = {
        .PipelineRenderingCreateInfo = {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
          .colorAttachmentCount = 1,
          .pColorAttachmentFormats = &swapchainFormat_s } },
      .UseDynamicRendering = true
    };
    ImGui_ImplVulkan_Init(&ii);
    
    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart);
    debugPrint(DebugSeverity::Verbose, "Initialized ImGui for Vulkan (took %.2fms)", (double)timeElapsed.count() / 1000.0);
  }

  auto totalTimeEnd = std::chrono::high_resolution_clock::now();
  auto totalTimeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(totalTimeEnd-totalTimeStart);
  DEBUG_VERBOSE("Finished initializing HLGL context using Vulkan (took %.2fms)", ((double)totalTimeElapsed.count() / 1000.0));
  initSuccess_s = true;
  return true;
}

void hlgl::shutdownContext() {
  if (device_s) {
    vkDeviceWaitIdle(device_s);

    ImGui_ImplVulkan_Shutdown();
    #if defined HLGL_WINDOW_LIBRARY_GLFW
      ImGui_ImplGlfw_Shutdown();
    #elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
      ImGui_ImplWin32_Shutdown();
    #endif
    ImGui::DestroyContext();

    defaultTextureNull_s.reset();
    defaultTextureWhite_s.reset();
    defaultTextureGray_s.reset();
    defaultTextureBlack_s.reset();
    stagingBuffer_s.reset();

    if (pipeLayout_s) vkDestroyPipelineLayout(device_s, pipeLayout_s, nullptr); pipeLayout_s = nullptr;
    if (descPool_s) vkDestroyDescriptorPool(device_s, descPool_s, nullptr); descPool_s = nullptr;
    for (VkDescriptorSetLayout& layout : descLayouts_s) {
      if (layout) vkDestroyDescriptorSetLayout(device_s, layout, nullptr); layout = nullptr;
    }
    
    for (size_t i {0}; i < numFramesInFlight_c; ++i) {
      if (frameFences_s[i]) { vkDestroyFence(device_s, frameFences_s[i], nullptr); frameFences_s[i] = nullptr; }
      if (acquireSemaphores_s[i]) { vkDestroySemaphore(device_s, acquireSemaphores_s[i], nullptr); acquireSemaphores_s[i] = nullptr; }
      if (frameCmdBuffers_s[i] && cmdPoolGraphics_s) { vkFreeCommandBuffers(device_s, cmdPoolGraphics_s, 1, &frameCmdBuffers_s[i]); frameCmdBuffers_s[i] = nullptr; }
    }
    for (VkSemaphore submitSemaphore : submitSemaphores_s) {
      if (submitSemaphore)
        vkDestroySemaphore(device_s, submitSemaphore, nullptr);
    }

    if (cmdPoolGraphics_s) { vkDestroyCommandPool(device_s, cmdPoolGraphics_s, nullptr); cmdPoolGraphics_s = nullptr; }
    if (cmdPoolTransfer_s) { vkDestroyCommandPool(device_s, cmdPoolTransfer_s, nullptr); cmdPoolTransfer_s = nullptr; }

    submitSemaphores_s.clear();
    swapchainImages_s.clear();
    // The swapchain textures have been added to the deletion queue after we already flushed it, so flush it again here.
    flushAllDelQueues();
    if (swapchain_s) { vkDestroySwapchainKHR(device_s, swapchain_s, nullptr); swapchain_s = nullptr; }
    if (allocator_s) { vmaDestroyAllocator(allocator_s); allocator_s = nullptr; }
    vkDestroyDevice(device_s, nullptr); device_s = nullptr;
    physicalDevice_s = nullptr;
  }
  if (instance_s) {
    if (surface_s) { vkDestroySurfaceKHR(instance_s, surface_s, nullptr); surface_s = nullptr; }
    if (debug_s) { vkDestroyDebugUtilsMessengerEXT(instance_s, debug_s, nullptr); debug_s = nullptr; }
    vkDestroyInstance(instance_s, nullptr); instance_s = nullptr;
  }
}

const hlgl::GpuProperties& hlgl::getGpuProperties() {
  return gpu_s;
}

float hlgl::getDisplayAspectRatio() {
  return static_cast<float>(displayWidth_s) / std::max<float>(1.0f, static_cast<float>(displayHeight_s));
}

hlgl::ImageFormat hlgl::getDisplayFormat() {
  return translate(swapchainFormat_s);
}

void hlgl::getDisplaySize(uint32_t& w, uint32_t& h) {
  w = displayWidth_s;
  h = displayHeight_s;
}

void hlgl::setDisplaySize(uint32_t w, uint32_t h) {
  displayWidth_s = w;
  displayHeight_s = h;
}

void hlgl::setVsync(VsyncMode mode) {
  vsync_s = mode;
}

void hlgl::setHdr(bool val) {
  hdr_s = val;
}

bool hlgl::isDepthFormatSupported(ImageFormat format) {
  VkFormatProperties2 formatProperties {.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
  vkGetPhysicalDeviceFormatProperties2(physicalDevice_s, translate(format), &formatProperties);
  return (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void hlgl::imguiNewFrame() {
  ImGui_ImplVulkan_NewFrame();
#if defined HLGL_WINDOW_LIBRARY_GLFW
  ImGui_ImplGlfw_NewFrame();
#elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
  ImGui_ImplWin32_NewFrame();
#endif
  ImGui::NewFrame();
}

bool hlgl::beginFrame() {
  if (inFrame_s) {
    DEBUG_ERROR("Can't begin a new frame while an existing frame is active.");
    return false;
  }

  // Get the command buffer and sync structures for the current frame in flight.
  frame_s.cmd = frameCmdBuffers_s[frameIndex_s];
  frame_s.fence = frameFences_s[frameIndex_s];
  frame_s.acquireSemaphore = acquireSemaphores_s[frameIndex_s];

  // Block until the previous commands sent to this frame are finished.
  if (!VKCHECK(vkWaitForFences(device_s, 1, &frame_s.fence, true, UINT64_MAX)))
    return false;
  
  // Resize the swapchain if neccessary.  This may abort the current frame, returning nullptr.
  {
    VkExtent2D checkExtent;
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_s, surface_s, &caps);
    if (caps.currentExtent.width != UINT32_MAX && caps.currentExtent.height != UINT32_MAX)
      checkExtent = caps.currentExtent;
    else {
      checkExtent.width = std::clamp<uint32_t>(displayWidth_s, caps.minImageExtent.width, caps.maxImageExtent.width);
      checkExtent.height = std::clamp<uint32_t>(displayHeight_s, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // If the display size has been changed, we'll have to recreate the swapchain.
    // This also checks to see if the HDR or Vsync state has changed, which also require a swapchain rebuild.
    if ( swapchainNeedsRebuild_s ||
        (checkExtent.width != swapchainExtent_s.width) ||
        (checkExtent.height != swapchainExtent_s.height) ||
        (swapchainPresentMode_s != translate(vsync_s)) ||
        (hdr_s != isHdrSurfaceFormat(swapchainFormat_s)))
    {
      // If the window has 0 width or height, bail out here.
      if (checkExtent.width == 0 || checkExtent.height == 0)
        return false;

      // Recreate the swapchain.
      buildSwapchain();

      // Notify any observers that the display has been resized.
      subjectDisplayResized_s.execute(swapchainExtent_s.width, swapchainExtent_s.height);

      // Reset this flag, which is set to true if VkAquireNextImageKHR ever returns VK_ERROR_OUT_OF_DATE_KHR.
      swapchainNeedsRebuild_s = false;
    }
    // The swapchain hasn't been resized, but we'll check for 0 just in case.
    else if (swapchainExtent_s.width == 0 || swapchainExtent_s.height == 0)
      return false;

    // The swapchain hasn't been resized and its size is non-zero, so we're good to go.
  }

  // Reset the in-flight fence.
  if (!VKCHECK(vkResetFences(device_s, 1, &frame_s.fence)))
    return false;

  // Get the next image index.
  {
    VkResult result;
    if (!VKCHECK_SWAPCHAIN(result = vkAcquireNextImageKHR(device_s, swapchain_s, UINT64_MAX, acquireSemaphores_s[frameIndex_s], nullptr, &swapchainIndex_s)))
      return false;
    if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
      swapchainNeedsRebuild_s = true;
      return false;
    }
  }

  frame_s.submitSemaphore = submitSemaphores_s[swapchainIndex_s];
  frame_s.swapchainImage = &swapchainImages_s[swapchainIndex_s];

  // Reset this frame's command buffer from its previous usage.
  if (!VKCHECK(vkResetCommandBuffer(frame_s.cmd, 0)))
    return false;

  // Delete any objects that were destroyed on this frame after the command buffer's been reset.
  flushDelQueue();

  // Begin recording commands.
  VkCommandBufferBeginInfo info {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    //.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };
  if (!VKCHECK(vkBeginCommandBuffer(frame_s.cmd, &info)))
    return false;

  frame_s.boundPipeline = nullptr;
  frame_s.boundIndexBuffer = nullptr;
  frame_s.boundIndexBufferOffset = 0;
  frame_s.frameCounter = frameCounter_s;
  frame_s.frameIndex = frameIndex_s;
  frame_s.inDrawingPass = false;

  vkCmdBindDescriptorSets(frame_s.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeLayout_s, 0, NUM_DESCRIPTOR_SETS, descSets_s.data(), 0, nullptr);
  vkCmdBindDescriptorSets(frame_s.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout_s, 0, NUM_DESCRIPTOR_SETS, descSets_s.data(), 0, nullptr);

  inFrame_s = true;
  return true;
}

void hlgl::endFrame() {
  if (!inFrame_s) {
    DEBUG_ERROR("Trying to end a frame before beginning one.");
    return;
  }
  Frame* frame = getCurrentFrame();

  // If we started a draw pass, end it here.
  endDrawing();

  // Draw the ImGUI frame to a custom drawing pass.
  beginDrawing({{frame->swapchainImage}});
  ImDrawData* drawData = ImGui::GetDrawData();
  if (drawData)
    ImGui_ImplVulkan_RenderDrawData(drawData, frame->cmd, nullptr);
  endDrawing();

  // Transition the swapchain texture to a presentable state.
  frame->swapchainImage->_pimpl->barrier(frame->cmd,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    VK_ACCESS_NONE,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

  // End the command buffer.
  vkEndCommandBuffer(frame->cmd);
  inFrame_s = false;

  // Submit the command buffer to the graphics queue.
  VkPipelineStageFlags waitStages {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSubmitInfo si {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &frame->acquireSemaphore,
    .pWaitDstStageMask = &waitStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &frame->cmd,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &frame->submitSemaphore };
  if (!VKCHECK(vkQueueSubmit(graphicsQueue_s, 1, &si, frame->fence)))
    return;

  // Present the image to the screen.
  VkPresentInfoKHR pi {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &frame->submitSemaphore,
    .swapchainCount = 1,
    .pSwapchains = &swapchain_s,
    .pImageIndices = &swapchainIndex_s
  };
  if (!VKCHECK_SWAPCHAIN(vkQueuePresentKHR(presentQueue_s, &pi)))
    return;

  // Advance the frame index for the next frame.
  frameIndex_s = (frameIndex_s + 1) % numFramesInFlight_c;
  ++frameCounter_s;
}


///////////////////////////////////////////////////////////////////////////////
// Implementation for functions declared in vk/context.h

VkDevice hlgl::getDevice() { return device_s; }
VmaAllocator hlgl::getAllocator() { return allocator_s; }

hlgl::Frame* hlgl::getCurrentFrame() { return (inFrame_s) ? &frame_s : nullptr; }

VkQueue hlgl::getGraphicsQueue() { return graphicsQueue_s; }
VkQueue hlgl::getPresentQueue() { return presentQueue_s; }

const std::array<VkDescriptorSetLayout,3>& hlgl::getDescSetLayouts() { return descLayouts_s; }
VkDescriptorSet hlgl::getDescriptorSet(uint32_t set) { return descSets_s[set]; }

uint32_t hlgl::allocDescriptorIndex(uint32_t set) {
  if (descFreeIndices_s[set].size() > 0) {
    uint32_t index = descFreeIndices_s[set].back();
    descFreeIndices_s[set].pop_back();
    return index;
  }
  else
    return descNextIndex_s[set]++;
}

VkPipelineLayout hlgl::getPipelineLayout() { return pipeLayout_s; }

hlgl::Texture* hlgl::getDefaultTextureNull()  { return &*defaultTextureNull_s; }
hlgl::Texture* hlgl::getDefaultTextureWhite() { return &*defaultTextureWhite_s; }
hlgl::Texture* hlgl::getDefaultTextureGray()  { return &*defaultTextureGray_s; }
hlgl::Texture* hlgl::getDefaultTextureBlack() { return &*defaultTextureBlack_s; }


VkCommandBuffer hlgl::beginImmediateCmd() {
  VkCommandBuffer cmd {nullptr};
  VkCommandBufferAllocateInfo ai {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = cmdPoolGraphics_s,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1 };
  if (!VKCHECK(vkAllocateCommandBuffers(device_s, &ai, &cmd)) || !cmd)
    return nullptr;
  
  VkCommandBufferBeginInfo bi {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  vkBeginCommandBuffer(cmd, &bi);
  return cmd;
}

void hlgl::submitImmediateCmd(VkCommandBuffer cmd) {
  vkEndCommandBuffer(cmd);
  VkSubmitInfo si {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd };
  VKCHECK(vkQueueSubmit(graphicsQueue_s, 1, &si, nullptr));
  VKCHECK(vkQueueWaitIdle(graphicsQueue_s));
  vkFreeCommandBuffers(device_s, cmdPoolGraphics_s, 1, &cmd);
}

void hlgl::queueDeletion(DelQueueItem item) {
  delQueues_s.back().push_back(item);
}

void hlgl::flushDelQueue() {

  for (auto& varItem : delQueues_s.front()) {
    if (std::holds_alternative<DelQueueBuffer>(varItem)) {
      auto item = std::get<DelQueueBuffer>(varItem);
      if (item.allocation && item.buffer) vmaDestroyBuffer(allocator_s, item.buffer, item.allocation);
    }
    else if (std::holds_alternative<DelQueueTexture>(varItem)) {
      auto item = std::get<DelQueueTexture>(varItem);
      if (item.view) vkDestroyImageView(device_s, item.view, nullptr);
      if (item.sampler) vkDestroySampler(device_s, item.sampler, nullptr);
      if (item.allocation && item.image) vmaDestroyImage(allocator_s, item.image, item.allocation);
    }
    else if (std::holds_alternative<DelQueuePipeline>(varItem)) {
      auto item = std::get<DelQueuePipeline>(varItem);
      if (item.pipeline) vkDestroyPipeline(device_s, item.pipeline, nullptr);
      if (item.layout) vkDestroyPipelineLayout(device_s, item.layout, nullptr);
    }
    else if (std::holds_alternative<DelQueueDescriptor>(varItem)) {
      auto item = std::get<DelQueueDescriptor>(varItem);
      descFreeIndices_s[item.set].push_back(item.index);
    }
  }
  for (size_t i {0}; (i+1) < numDelQueues_c; ++i) {
    delQueues_s[i] = std::move(delQueues_s[i+1]);
  }
  delQueues_s.back() = {};
}

void hlgl::flushAllDelQueues() {
  for (size_t i {0}; i < numDelQueues_c; ++i) {
    flushDelQueue();
  }
}

void hlgl::observeDisplayResize(Observer<uint32_t,uint32_t>* observer, std::function<void(uint32_t,uint32_t)> callback) {
  subjectDisplayResized_s.attach(observer, callback);
}

void hlgl::transferToStagingBuffer(const void* srcMem, size_t srcOffset, size_t size) {
  // We can re-use the same staging buffer for multiple transfers in a single frame, as long as the memory regions don't overlap.
  if (stagingBufferLastFrameUsed_s != frameCounter_s) {
    stagingBufferOffset_s = 0;
    stagingBufferLastFrameUsed_s = frameCounter_s;
  }
  else {
    // Round the offset up to the nearest multiple of 16 bytes.  This alignment is required for some copy operations.
    stagingBufferOffset_s = (stagingBufferOffset_s + 15) & ~15;
  }
  // If we run out of space, destroy the existing staging buffer and create a new, bigger one.
  // the underlying VkBuffer is put into a queue and destroyed on a later frame, so this wont break any active/pending transfers.
  if (stagingBufferOffset_s + size > stagingBuffer_s->getSize()) {
    DeviceSize buffSize {stagingBuffer_s->getSize()};
    do { buffSize *= 2; } while (buffSize < size);
    stagingBuffer_s.reset();
    stagingBuffer_s.emplace(Buffer::CreateParams{
      .usage = BufferUsage::TransferSrc | BufferUsage::HostVisible,
      .size = buffSize });
    stagingBufferOffset_s = 0;
  }
  // Once offsets and sizes are checked, a regular mem-to-buffer transfer copies the data into place.
  transfer(stagingBuffer_s->_pimpl.get(), stagingBufferOffset_s, srcMem, srcOffset, size, false);
}

void hlgl::transfer(BufferImpl* dstBuffer, DeviceSize dstOffset, const void* srcMem, size_t srcOffset, size_t size, bool useTransferQueue) {
  if (!dstBuffer) {
    DEBUG_ERROR("Invalid dstBuffer for 'transfer'.");
    return;
  }
  Frame* frame {getCurrentFrame()};

  // if dstBuffer is hostVisible, then useTransferQueue is ignored because we're using memcpy to directly transfer to the buffer.
  if (dstBuffer->hostVisible) {

    if (dstBuffer->fifSynced && frame) {
      // If this buffer is fif synced AND we're currently inside a frame, transfer only to the "current" buffer.
      memcpy((uint8_t*)(dstBuffer->allocInfo[frame->frameIndex].pMappedData) + dstOffset, (uint8_t*)(srcMem) + srcOffset, size);
    }
    else {
      // if this buffer is not fif synced or we're outside a frame, transfer to one or both "sides" of the buffer.
      for (size_t i {0}; i < (dstBuffer->fifSynced ? 2 : 1); ++i) {
        memcpy((uint8_t*)(dstBuffer->allocInfo[i].pMappedData) + dstOffset, (uint8_t*)(srcMem) + srcOffset, size);
      }
    }
  }
  // If dstBuffer is NOT hostVisible, then we'll have to use the staging buffer as a go-between.
  else {
    transferToStagingBuffer(srcMem, srcOffset, size);
    transfer(dstBuffer, dstOffset, stagingBuffer_s->_pimpl.get(), stagingBufferOffset_s, size, useTransferQueue);
    stagingBufferOffset_s += size;
  }
}

void hlgl::transfer(BufferImpl* dstBuffer, DeviceSize dstOffset, BufferImpl* srcBuffer, DeviceSize srcOffset, DeviceSize size, bool useTransferQueue) {
  if (!dstBuffer) {
    DEBUG_ERROR("Invalid dstBuffer for 'transfer'.");
    return;
  }
  if (!srcBuffer) {
    DEBUG_ERROR("Invalid srcBuffer for 'transfer'.");
    return;
  }
  Frame* frame {getCurrentFrame()};
  VkCommandBuffer cmd = frame ? frame->cmd : beginImmediateCmd();
  VkBufferCopy info{.srcOffset = srcOffset, .dstOffset = dstOffset, .size = size};
  // If we are in a frame, then this will transfer from src's current buffer (or 0 if not synced) to dst's current buffer (or 0 if not synced).
  vkCmdCopyBuffer(cmd, srcBuffer->getBuffer(frame), dstBuffer->getBuffer(frame), 1, &info);
  // If we are NOT in a frame, then the above will transfer from src's 0 to dst's 0, regardless of whether either is synced.
  if (!frame) {
    if (dstBuffer->fifSynced) {
      // If dst is synced, copy from src's 0 (or 1 if src is synced) into dst's 1.
      // If src is synced by dst is not, don't bother messing with it.
      vkCmdCopyBuffer(cmd, srcBuffer->buffer[srcBuffer->fifSynced ? 1 : 0], dstBuffer->buffer[1], 1, &info);
    }
    submitImmediateCmd(cmd);
  }

  // TODO: Make use of the transfer queue!
}

void hlgl::transfer(TextureImpl* dstTexture, DeviceSize dstOffset, const void* srcMem, size_t srcOffset, DeviceSize size, bool useTransferQueue) {
  // Images are always created using TILING_OPTIMAL, so we can never memcpy directly into them.  The staging buffer is mandatory.
  transferToStagingBuffer(srcMem, srcOffset, size);
  transfer(dstTexture, dstOffset, stagingBuffer_s->_pimpl.get(), stagingBufferOffset_s, size, useTransferQueue);
  stagingBufferOffset_s += size;
}

void hlgl::transfer(TextureImpl* dstTexture, DeviceSize dstOffset, BufferImpl* srcBuffer, DeviceSize srcOffset, DeviceSize size, bool useTransferQueue) {
  if (!dstTexture) {
    DEBUG_ERROR("Invalid dstTexture for 'transfer'.");
    return;
  }
  if (!srcBuffer) {
    DEBUG_ERROR("Invalid srcBuffer for 'transfer'.");
    return;
  }
  VkCommandBuffer cmd = beginImmediateCmd();
  dstTexture->barrier(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkBufferImageCopy copy = {
    .bufferOffset = srcOffset,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource = {
      .aspectMask = translateAspect(dstTexture->format),
      .mipLevel = dstTexture->mipBase,
      .baseArrayLayer = dstTexture->layerBase,
      .layerCount = dstTexture->layerCount },
    .imageExtent = dstTexture->extent };
  vkCmdCopyBufferToImage(cmd, srcBuffer->getBuffer(nullptr), dstTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  dstTexture->barrier(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
  submitImmediateCmd(cmd);
}