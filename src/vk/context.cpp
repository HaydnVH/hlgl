#include "vkimpl-context.h"
#include "vkimpl-debug.h"
#include "vkimpl-translate.h"
#include <hlgl/context.h>
#include <hlgl/frame.h>

#include "../utils/array.h"
#include <algorithm>
#include <map>
#include <set>
#include <vector>

#if defined HLGL_WINDOW_LIBRARY_GLFW
#include <GLFW/glfw3.h>
#elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
#include <Windows.h>
#endif // defined HLGL_WINDOW_LIBRARY_x

namespace {

  hlgl::DebugCallback debugCallback_s {};
  bool initSuccess_s {false};
  bool inFrame_s {false};
  hlgl::GpuProperties gpu_s;
  uint32_t displayWidth_s {0}, displayHeight_s {0};
  hlgl::VsyncMode vsync_s {hlgl::VsyncMode::Fifo};
  bool hdr_s {false};
  
  VkInstance instance_s {nullptr};
  VkDebugUtilsMessengerEXT debug_s {nullptr};
  VkSurfaceKHR surface_s {nullptr};
  VkPhysicalDevice physicalDevice_s {nullptr};
  VkDevice device_s {nullptr};

  uint32_t graphicsQueueFamily_s {UINT32_MAX};
  uint32_t presentQueueFamily_s {UINT32_MAX};
  uint32_t computeQueueFamily_s {UINT32_MAX};
  uint32_t transferQueueFamily_s {UINT32_MAX};
  VkQueue graphicsQueue_s {nullptr};
  VkQueue presentQueue_s {nullptr};
  VkQueue computeQueue_s {nullptr};
  VkQueue transferQueue_s {nullptr};

  VkCommandPool cmdPool_s {nullptr};

  VkDescriptorPool descPool_s {nullptr};
  constexpr uint32_t imageSamplerDescPoolSize_c {10000};
  VkDescriptorSetLayout imageSamplerDescSetLayout_s {nullptr};
  VkDescriptorSet imageSamplerDescSet_s {nullptr};

  VkSwapchainKHR swapchain_s {nullptr};
  VkExtent2D swapchainExtent_s {};
  VkFormat swapchainFormat_s {};
  VkPresentModeKHR swapchainPresentMode_s {VK_PRESENT_MODE_FIFO_KHR};
  uint32_t swapchainIndex_s {0};
  bool swapchainNeedsRebuild_s {false};
  std::vector<hlgl::Texture> swapchainTextures_s;
  std::set<hlgl::Texture*> screenSizeTextures_s;

  constexpr size_t numFramesInFlight_c {2};
  std::array<hlgl::_impl::FrameInFlight, numFramesInFlight_c> frames_s;
  uint32_t frameIndex_s {0};
  
  VmaAllocator allocator_s {nullptr};

  std::array<std::vector<hlgl::_impl::DelQueueItem>, 3> delQueues_s;

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
        hlgl::debugPrint(hlgl::DebugSeverity::Verbose, std::format("[VK] {}", data->pMessage));
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        hlgl::debugPrint(hlgl::DebugSeverity::Info, std::format("[VK] {}", data->pMessage));
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        hlgl::debugPrint(hlgl::DebugSeverity::Warning, std::format("[VK] {}", data->pMessage));
        break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        hlgl::debugPrint(hlgl::DebugSeverity::Error, std::format("[VK] {}", data->pMessage));
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
        debugPrint(DebugSeverity::Warning, "HDR was requested but an appropriate HDR surface format could not be found.");
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
        debugPrint(DebugSeverity::Warning, std::format("Desired Vsync mode '{}' is unavailable, using FIFO instead.", toStr(vsync_s)));
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
    swapchainTextures_s.clear();
    if (swapchain_s)
      vkDestroySwapchainKHR(device_s, swapchain_s, nullptr);

    // Save the new swapchain.
    swapchain_s = newSwapchain;
    if (swapchain_s == nullptr) {
      debugPrint(DebugSeverity::Error, "Failed to create swapchain.");
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

    debugPrint(DebugSeverity::Verbose, std::format("Created swapchain ({} x {}) ({} - {} - {})",
      displayWidth_s, displayHeight_s,
      string_VkFormat(surfaceFormat.format), string_VkColorSpaceKHR(surfaceFormat.colorSpace), string_VkPresentModeKHR(swapchainPresentMode_s)));

    // Get the swapchain images.
    uint32_t imageCount {0};
    vkGetSwapchainImagesKHR(device_s, swapchain_s, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(device_s, swapchain_s, &imageCount, images.data());

    // Use the images to create textures, which handle image views and such.
    swapchainTextures_s.reserve(images.size());
    for (size_t i {0}; i < images.size(); ++i) {
      swapchainTextures_s.emplace_back(TextureParams{
        .width = swapchainExtent_s.width,
        .height = swapchainExtent_s.height,
        .format = translate(surfaceFormat.format),
        .debugName = std::format("context.swapchain[{}]", i),
        ._existingImage = images[i]});
    }
    return true;
  }

} // namespace <anon>

void hlgl::debugPrint(hlgl::DebugSeverity severity, std::string_view message) {
  if (debugCallback_s)
    debugCallback_s(severity, message);
}

bool hlgl::context::init(InitParams params) {
  // Save the debug callback so the user can be notified of any errors.
  debugCallback_s = params.debugCallback;

  // Make sure we aren't trying to initialize more than once.
  if (initSuccess_s) {
    debugPrint(DebugSeverity::Fatal, "HLGL context cannot be initialized more than once.");
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
    // Initialize Volk to fetch extension function pointers and such.
    if (!VKCHECK(volkInitialize())) {
      debugPrint(DebugSeverity::Fatal, "Failed to initialize volk; no vulkan-capable drivers installed?");
      return false;
    }

    VkApplicationInfo appInfo {
      .pApplicationName = params.appName ? "" : params.appName,
      .applicationVersion = VK_MAKE_VERSION(params.appVerMajor, params.appVerMinor, params.appVerPatch),
      .pEngineName = params.engineName ? "" : params.engineName,
      .engineVersion = VK_MAKE_VERSION(params.engineVerMajor, params.engineVerMinor, params.engineVerPatch),
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
      debugPrint(DebugSeverity::Verbose, std::format("Found {}/{} required Vulkan layer(s):", reqLayersFound.size(), requiredLayers.size()));
      for (auto layer : reqLayersFound) { debugPrint(DebugSeverity::Verbose, std::format("  - {}", layer)); }
      if (reqLayersMissing.size() > 0) {
        debugPrint(DebugSeverity::Fatal, std::format("Missing required Vulkan layer(s):"));
        for (auto layer : reqLayersMissing) { debugPrint(DebugSeverity::Fatal, std::format("  - {}", layer)); }
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
      debugPrint(DebugSeverity::Verbose, std::format("Found {}/{} optional Vulkan layer(s):", optLayersFound.size(), optionalLayers.size()));
      for (auto layer : optLayersFound) { debugPrint(DebugSeverity::Verbose, std::format("  - {}", layer)); }
      if (optLayersMissing.size() > 0) {
        debugPrint(DebugSeverity::Verbose, std::format("Missing optional Vulkan layer(s):"));
        for (auto layer : optLayersMissing) { debugPrint(DebugSeverity::Verbose, std::format("  - {}", layer)); }
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

    debugPrint(DebugSeverity::Verbose, std::format("Found {}/{} required Vulkan instance extension(s):", reqExtensionsFound.size(), requiredExtensions.size()));
    for (const char* extension : reqExtensionsFound) { debugPrint(DebugSeverity::Verbose, std::format("  - {}", extension)); }
    if (reqExtensionsMissing.size() > 0) {
      debugPrint(DebugSeverity::Fatal, std::format("Missing required Vulkan instance extension(s):"));
      for (const char* extension : reqExtensionsMissing) { debugPrint(DebugSeverity::Fatal, std::format("  - {}", extension)); }
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

    debugPrint(DebugSeverity::Verbose, std::format("Found {}/{} optional Vulkan instance extension(s):", optExtensionsFound.size(), optionalExtensions.size()));
    for (const char* extension : optExtensionsFound) { debugPrint(DebugSeverity::Verbose, std::format("  - {}", extension)); }
    if (optExtensionsMissing.size() > 0) {
      debugPrint(DebugSeverity::Verbose, std::format("Missing optional Vulkan instance extension(s):"));
      for (const char* extension : optExtensionsMissing) { debugPrint(DebugSeverity::Verbose, std::format("  - {}", extension)); }
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
    debugPrint(DebugSeverity::Verbose, "Created Vulkan instance.");
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Debugging
  if (gpu_s.enabledFeatures & Feature::Validation) {
    VkDebugUtilsMessengerCreateInfoEXT ci = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType =     VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = vulkanDebugCallback };

    if (!VKCHECK(vkCreateDebugUtilsMessengerEXT(instance_s, &ci, nullptr, &debug_s)) || !debug_s) {
      debugPrint(DebugSeverity::Warning, "Failed to create Vulkan debug messenger.");
      gpu_s.enabledFeatures ^= (~Feature::Validation);
    }

    debugPrint(DebugSeverity::Verbose, "Created Vulkan debug messenger.");
  }

  /////////////////////////////////////////////////////////////////////////////
  // Create Surface
  {
    #if defined HLGL_WINDOW_LIBRARY_GLFW
    if (!VKCHECK(glfwCreateWindowSurface(instance_s, params.window, nullptr, &surface_s)) || !surface_s) {
      debugPrint(DebugSeverity::Fatal, "Failed to create Vulkan window surface for GLFW.");
      return false;
    }
    #elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
    {
      VkWin32SurfaceCreateInfoKHR ci {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandle(nullptr),
        .hwnd = params.window };
      if (!VKCHECK(vkCreateWin32SurfaceKHR(instance_s, &ci, nullptr, &surface_s)) || !surface_s) {
        debugPrint(DebugSeverity::Fatal, "Failed to create Vulkan window surface for Win32.");
        return false;
      }
    }
    #endif

    debugPrint(DebugSeverity::Verbose, "Created Vulkan window surface.");
  }

  /////////////////////////////////////////////////////////////////////////////
  // Device Extensions
  constexpr size_t maxDeviceExtensions_c {32};
  hlgl::Array<const char*, maxDeviceExtensions_c> requiredDeviceExtensions;
  hlgl::Array<const char*, maxDeviceExtensions_c> optionalDeviceExtensions;
  std::vector<VkExtensionProperties> extensionProperties;
  std::vector<VkQueueFamilyProperties> queueFamilyProperties;
  {
    requiredDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    requiredDeviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

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
      debugPrint(DebugSeverity::Verbose, std::format("Found physical device '{}', checking properties...",
        pdProperties.deviceName));

      // Make sure the neccessary queue families are supported.
      uint32_t graphicsFamily, presentFamily, computeFamily, transferFamily;
      getQueueFamilyIndices(physicalDevice, surface_s, graphicsFamily, presentFamily, computeFamily, transferFamily, queueFamilyProperties);
      if (graphicsFamily == UINT32_MAX ||
          presentFamily == UINT32_MAX ||
          computeFamily == UINT32_MAX ||
          transferFamily == UINT32_MAX)
      {
        debugPrint(DebugSeverity::Verbose, "  ...required queue familes not supported, skipping.");
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
          debugPrint(DebugSeverity::Verbose, std::format("  ...required device extension '{}' not supported, skipping.", extension));
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
        debugPrint(DebugSeverity::Verbose, "  ...no surface formats available, skipping.");
        continue;
      }

      // Make sure there's at least one supported present mode.
      uint32_t presentModesCount {0};
      vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface_s, &presentModesCount, nullptr);
      if (presentModesCount == 0) {
        debugPrint(DebugSeverity::Verbose, "  ...no surface present modes available, skipping.");
        continue;
      }

      // TODO: Check for feature support.

      // Pack it up and add it to the list.
      GpuPair pair {properties, physicalDevice};
      appropriateDevices.push_back(pair);
    }

    if (appropriateDevices.empty()) {
      debugPrint(DebugSeverity::Fatal, "Failed to find an appropriate GPU.");
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
        debugPrint(DebugSeverity::Warning, std::format("Couldn't find preferred GPU '{}', will choose the most appropriate device instead.", params.preferredGpu));
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

    debugPrint(DebugSeverity::Info, std::format("Using {} ({}) with {} bytes of device memory.",
      gpu_s.name, toStr(gpu_s.type), gpu_s.deviceMemory));
    debugPrint(DebugSeverity::Info, std::format("Vulkan API version {}.{}.{}, Driver version {}.{}.{}",
      gpu_s.apiVerMajor, gpu_s.apiVerMinor, gpu_s.apiVerPatch,
      gpu_s.driverVerMajor, gpu_s.driverVerMinor, gpu_s.driverVerPatch));
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Logical Device
  {
    debugPrint(DebugSeverity::Verbose, std::format("Found {} required Vulkan device extension(s):", requiredDeviceExtensions.size()));
    for (const char* extension : requiredDeviceExtensions) {
      debugPrint(DebugSeverity::Verbose, std::format("  - {}", extension));
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

    debugPrint(DebugSeverity::Verbose, std::format("Found {}/{} optional Vulkan device extension(s):",
      supportedOptionalExtensions.size(), optionalDeviceExtensions.size()));
    for (const char* extension : supportedOptionalExtensions) {
      debugPrint(DebugSeverity::Verbose, std::format("  - {}", extension));
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
    if (gpu_s .enabledFeatures & Feature ::MeshShading)
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
      .descriptorBindingSampledImageUpdateAfterBind = true,
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
      debugPrint(DebugSeverity::Fatal, "Failed to create Vulkan logical device.");
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
        debugPrint(DebugSeverity::Warning, "Failed to set Vulkan debug name for 'instance_s'.");
      
      info.objectType = VK_OBJECT_TYPE_SURFACE_KHR;
      info.objectHandle = (uint64_t)surface_s;
      info.pObjectName = "surface_s";
      if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        debugPrint(DebugSeverity::Warning, "Failed to set Vulkan debug name for 'surface_s'.");
      
      info.objectType = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
      info.objectHandle = (uint64_t)physicalDevice_s;
      info.pObjectName = "physicalDevice_s";
      if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        debugPrint(DebugSeverity::Warning, "Failed to set Vulkan debug name for 'physicalDevice_s'.");

      info.objectType = VK_OBJECT_TYPE_DEVICE;
      info.objectHandle = (uint64_t)device_s;
      info.pObjectName = "device_s";
      if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        debugPrint(DebugSeverity::Warning, "Failed to set Vulkan debug name for 'device_s'.");
    }

    debugPrint(DebugSeverity::Verbose, "Created Vulkan logical device.");
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Queues
  {
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
          debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", queueName));
      }
    }

    debugPrint(DebugSeverity::Verbose, std::format("Using Vulkan device queues with family indices: {}(graphics), {}(present), {}(compute), {}(transfer)",
      graphicsQueueFamily_s, presentQueueFamily_s, computeQueueFamily_s, transferQueueFamily_s));
  }
  
  /////////////////////////////////////////////////////////////////////////////
  // Initialize Vulkan Memory Allocator
  {
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
      debugPrint(DebugSeverity::Fatal, "Failed to create VMA allocator.");
      return false;
    }

    debugPrint(DebugSeverity::Verbose, "Created VMA allocator.");
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Command Pool
  {
    VkCommandPoolCreateInfo ci {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = graphicsQueueFamily_s };
    
    if (!VKCHECK(vkCreateCommandPool(device_s, &ci, nullptr, &cmdPool_s)) || !cmdPool_s) {
      debugPrint(DebugSeverity::Fatal, "Failed to create Vulkan command pool.");
      return false;
    }

    if (gpu_s.enabledFeatures & Feature::Validation) {
      VkDebugUtilsObjectNameInfoEXT info {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_COMMAND_POOL,
        .objectHandle = (uint64_t)cmdPool_s,
        .pObjectName = "cmdPool_s" };
      if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        debugPrint(DebugSeverity::Warning, "Failed to set Vulkan debug name for 'cmdPool_s'.");
    }

    debugPrint(DebugSeverity::Verbose, "Created Vulkan command pool.");
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Descriptor Pool
  {
    VkDescriptorBindingFlags descBindFlags {
      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
      VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo descBindFlagsInfo {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
      .bindingCount = 1,
      .pBindingFlags = &descBindFlags };
    VkDescriptorSetLayoutBinding descBind {
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = imageSamplerDescPoolSize_c,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT };
    VkDescriptorSetLayoutCreateInfo descSetLayoutInfo {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = &descBindFlagsInfo,
      .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
      .bindingCount = 1,
      .pBindings = &descBind };
    if (!VKCHECK(vkCreateDescriptorSetLayout(device_s, &descSetLayoutInfo, nullptr, &imageSamplerDescSetLayout_s)) || !imageSamplerDescSetLayout_s) {
      debugPrint(DebugSeverity::Error, "Failed to create Vulkan descriptor set layout.");
      return false;
    }

    VkDescriptorPoolSize poolSizes[] {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageSamplerDescPoolSize_c},
    };

    VkDescriptorPoolCreateInfo ci {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = poolSizes };
    if (!VKCHECK(vkCreateDescriptorPool(device_s, &ci, nullptr, &descPool_s)) || !descPool_s) {
      debugPrint(DebugSeverity::Fatal, "Failed to create Vulkan descriptor pool.");
      return false;
    }

    uint32_t variableDescCount {imageSamplerDescPoolSize_c};
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountInfo {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
      .descriptorSetCount = 1,
      .pDescriptorCounts = &variableDescCount };
    VkDescriptorSetAllocateInfo descSetAlloc {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = &variableDescCountInfo,
      .descriptorPool = descPool_s,
      .descriptorSetCount = 1,
      .pSetLayouts = &imageSamplerDescSetLayout_s };
    if (!VKCHECK(vkAllocateDescriptorSets(device_s, &descSetAlloc, &imageSamplerDescSet_s)) || !imageSamplerDescSet_s) {
      debugPrint(DebugSeverity::Fatal, "Failed to allocate descriptor set.");
      return false;
    }

    if (gpu_s.enabledFeatures & Feature::Validation) {
      VkDebugUtilsObjectNameInfoEXT info {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_DESCRIPTOR_POOL,
        .objectHandle = (uint64_t)descPool_s,
        .pObjectName = "descPool_s" };
      if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
        debugPrint(DebugSeverity::Warning, "Failed to set Vulkan debug name for 'descPool_s'.");
    }

    debugPrint(DebugSeverity::Warning, "Created Vulkan descriptor pool.");
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Swapchain
  if (!buildSwapchain() || !swapchain_s) {
    debugPrint(DebugSeverity::Fatal, "Failed to create swapchain.");
    return false;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize Frame Synchronization
  {
    for (uint32_t i {0}; i < numFramesInFlight_c; ++i) {
      VkCommandBufferAllocateInfo ai {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmdPool_s,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1 };
      if (!VKCHECK(vkAllocateCommandBuffers(device_s, &ai, &frames_s[i].cmd)))
        return false;

      VkSemaphoreCreateInfo sci {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
      if (!VKCHECK(vkCreateSemaphore(device_s, &sci, nullptr, &frames_s[i].imageAvailable)))
        return false;
      if (!VKCHECK(vkCreateSemaphore(device_s, &sci, nullptr, &frames_s[i].renderFinished)))
        return false;

      VkFenceCreateInfo fci {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT };
      if (!VKCHECK(vkCreateFence(device_s, &fci, nullptr, &frames_s[i].fence)))
        return false;

      if (gpu_s.enabledFeatures & Feature::Validation) {
        VkDebugUtilsObjectNameInfoEXT info = {
          .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        std::string debugName;
        info.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
        info.objectHandle = (uint64_t)frames_s[i].cmd;
        debugName = std::format("frames_s[{}].cmd", i);
        info.pObjectName = debugName.c_str();
        if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
          debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", debugName));

        info.objectType = VK_OBJECT_TYPE_SEMAPHORE;
        info.objectHandle = (uint64_t)frames_s[i].imageAvailable;
        debugName = std::format("frames_s[{}].imageAvailable", i);
        info.pObjectName = debugName.c_str();
        if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
          debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", debugName));

        info.objectType = VK_OBJECT_TYPE_SEMAPHORE;
        info.objectHandle = (uint64_t)frames_s[i].renderFinished;
        debugName = std::format("frames_s[{}].renderFinished", i);
        info.pObjectName = debugName.c_str();
        if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
          debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", debugName));

        info.objectType = VK_OBJECT_TYPE_FENCE;
        info.objectHandle = (uint64_t)frames_s[i].fence;
        debugName = std::format("frames_s[{}].fence", i);
        info.pObjectName = debugName.c_str();
        if (!VKCHECK_WARN(vkSetDebugUtilsObjectNameEXT(device_s, &info)))
          debugPrint(DebugSeverity::Warning, std::format("Failed to set Vulkan debug name for '{}'.", debugName));
      }
    }

    debugPrint(DebugSeverity::Verbose, "Created command buffers and synchronization primitives for frames in flight.");
  }

  /////////////////////////////////////////////////////////////////////////////
  // Initialize ImGui
  {
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
      .MinImageCount = 3,
      .ImageCount = 3,
      .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
      .DescriptorPoolSize = 1000,
      .UseDynamicRendering = true,
      .PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchainFormat_s }
    };
    ImGui_ImplVulkan_Init(&ii);
    ImGui_ImplVulkan_CreateFontsTexture();
    debugPrint(DebugSeverity::Verbose, "Initialized ImGui for Vulkan.");
  }

  debugPrint(DebugSeverity::Verbose, "Finished initializing Vulkan context for HLGL.");
  initSuccess_s = true;
  return true;
}

void hlgl::context::shutdown() {
  if (device_s) {
    vkDeviceWaitIdle(device_s);

    ImGui_ImplVulkan_Shutdown();
    #if defined HLGL_WINDOW_LIBRARY_GLFW
      ImGui_ImplGlfw_Shutdown();
    #elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
      ImGui_ImplWin32_Shutdown();
    #endif
    ImGui::DestroyContext();

    if (allocator_s) { vmaDestroyAllocator(allocator_s); allocator_s = nullptr; }
    for (auto& frame : frames_s) {
      if (frame.fence) { vkDestroyFence(device_s, frame.fence, nullptr); frame.fence = nullptr; }
      if (frame.renderFinished) { vkDestroySemaphore(device_s, frame.renderFinished, nullptr); frame.renderFinished = nullptr; }
      if (frame.imageAvailable) { vkDestroySemaphore(device_s, frame.imageAvailable, nullptr); frame.imageAvailable = nullptr; }
      if (frame.cmd && cmdPool_s) { vkFreeCommandBuffers(device_s, cmdPool_s, 1, &frame.cmd); frame.cmd = nullptr; }
    }
    swapchainTextures_s.clear();
    // The swapchain textures have been added to the deletion queue after we already flushed it, so flush it again here.
    _impl::flushAllDelQueues();
    if (swapchain_s) { vkDestroySwapchainKHR(device_s, swapchain_s, nullptr); swapchain_s = nullptr; }
    if (descPool_s) { vkDestroyDescriptorPool(device_s, descPool_s, nullptr); descPool_s = nullptr; }
    if (cmdPool_s) { vkDestroyCommandPool(device_s, cmdPool_s, nullptr); cmdPool_s = nullptr; }
    vkDestroyDevice(device_s, nullptr); device_s = nullptr;
    physicalDevice_s = nullptr;
  }
  if (instance_s) {
    if (surface_s) { vkDestroySurfaceKHR(instance_s, surface_s, nullptr); surface_s = nullptr; }
    if (debug_s) { vkDestroyDebugUtilsMessengerEXT(instance_s, debug_s, nullptr); debug_s = nullptr; }
    vkDestroyInstance(instance_s, nullptr); instance_s = nullptr;
  }
}

const hlgl::GpuProperties& hlgl::context::getGpuProperties() {
  return gpu_s;
}

float hlgl::context::getDisplayAspectRatio() {
  return static_cast<float>(displayWidth_s) / std::max<float>(1.0f, static_cast<float>(displayHeight_s));
}

hlgl::Format hlgl::context::getDisplayFormat() {
  return translate(swapchainFormat_s);
}

void hlgl::context::getDisplaySize(uint32_t& w, uint32_t& h) {
  w = displayWidth_s;
  h = displayHeight_s;
}

void hlgl::context::setDisplaySize(uint32_t w, uint32_t h) {
  displayWidth_s = w;
  displayHeight_s = h;
}

void hlgl::context::setVsync(VsyncMode mode) {
  vsync_s = mode;
}

void hlgl::context::setHdr(bool val) {
  hdr_s = val;
}

bool hlgl::context::isDepthFormatSupported(Format format) {
  VkFormatProperties2 formatProperties {.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
  vkGetPhysicalDeviceFormatProperties2(physicalDevice_s, translate(format), &formatProperties);
  return (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void hlgl::context::imguiNewFrame() {
  ImGui_ImplVulkan_NewFrame();
#if defined HLGL_WINDOW_LIBRARY_GLFW
  ImGui_ImplGlfw_NewFrame();
#elif defined HLGL_WINDOW_LIBRARY_NATIVE_WIN32
  ImGui_ImplWin32_NewFrame();
#endif
  ImGui::NewFrame();
}

///////////////////////////////////////////////////////////////////////////////
// This is the end of interface implementations.
// Beyond this point is functions only used interally by HLGL.

VkDevice hlgl::_impl::getDevice() {
  return device_s;
}

VmaAllocator hlgl::_impl::getAllocator() {
  return allocator_s;
}

bool hlgl::_impl::isValidationEnabled() {
  return (gpu_s.enabledFeatures & Feature::Validation);
}

hlgl::_impl::FrameInFlight& hlgl::_impl::getCurrentFrameInFlight() {
  return frames_s[frameIndex_s];
}

uint32_t hlgl::_impl::getFrameIndex() {
  return frameIndex_s;
}

hlgl::Texture& hlgl::_impl::getCurrentSwapchainTexture() {
  return swapchainTextures_s[swapchainIndex_s];
}

VkSwapchainKHR hlgl::_impl::getSwapchain() {
  return swapchain_s;
}

uint32_t hlgl::_impl::getSwapchainIndex() {
  return swapchainIndex_s;
}

void hlgl::_impl::advanceFrame() {
  frameIndex_s = (frameIndex_s + 1) % numFramesInFlight_c;
}

bool hlgl::_impl::acquireNextImage() {
  // TODO: Check for "VK_ERROR_OUT_OF_DATE_KHR" which signals that the swapchain needs to be rebuilt.
  VkResult result;
  if (!VKCHECK_SWAPCHAIN(result = vkAcquireNextImageKHR(device_s, swapchain_s, UINT64_MAX, frames_s[frameIndex_s].imageAvailable, nullptr, &swapchainIndex_s)))
    return false;
  if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
    swapchainNeedsRebuild_s = true;
    return false;
  }
  else
    return true;
}

VkQueue hlgl::_impl::getGraphicsQueue() {
  return graphicsQueue_s;
}

VkQueue hlgl::_impl::getPresentQueue() {
  return presentQueue_s;
}

VkCommandBuffer hlgl::_impl::beginImmediateCmd() {
  VkCommandBuffer cmd {nullptr};
  VkCommandBufferAllocateInfo ai {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = cmdPool_s,
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

void hlgl::_impl::submitImmediateCmd(VkCommandBuffer cmd) {
  vkEndCommandBuffer(cmd);
  VkSubmitInfo si {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd };
  VKCHECK(vkQueueSubmit(graphicsQueue_s, 1, &si, nullptr));
  VKCHECK(vkQueueWaitIdle(graphicsQueue_s));
  vkFreeCommandBuffers(device_s, cmdPool_s, 1, &cmd);
}

void hlgl::_impl::queueDeletion(DelQueueItem item) {
  delQueues_s.back().push_back(item);
}

void hlgl::_impl::flushDelQueue() {

  for (auto& varItem : delQueues_s.front()) {
    if (std::holds_alternative<DelQueueBuffer>(varItem)) {
      auto item = std::get<DelQueueBuffer>(varItem);
      if (item.allocation && item.buffer) vmaDestroyBuffer(allocator_s, item.buffer, item.allocation);
    }
    else if (std::holds_alternative<DelQueueTexture>(varItem)) {
      auto item = std::get<DelQueueTexture>(varItem);
      if (item.sampler) vkDestroySampler(device_s, item.sampler, nullptr);
      if (item.view) vkDestroyImageView(device_s, item.view, nullptr);
      if (item.allocation && item.image) vmaDestroyImage(allocator_s, item.image, item.allocation);
    }
    else if (std::holds_alternative<DelQueuePipeline>(varItem)) {
      auto item = std::get<DelQueuePipeline>(varItem);
      if (item.pipeline) vkDestroyPipeline(device_s, item.pipeline, nullptr);
      if (item.layout) vkDestroyPipelineLayout(device_s, item.layout, nullptr);
      if (item.descLayout) vkDestroyDescriptorSetLayout(device_s, item.descLayout, nullptr);
    }
  }
  for (size_t i {0}; (i+1) < delQueues_s.size(); ++i) {
    delQueues_s[i] = std::move(delQueues_s[i+1]);
  }
  delQueues_s.back() = {};
}

void hlgl::_impl::flushAllDelQueues() {
  for (size_t i {0}; i < delQueues_s.size(); ++i) {
    flushDelQueue();
  }
}

bool hlgl::_impl::resizeIfNeeded() {
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

    // Rebuild screen-sized textures.
    for (hlgl::Texture* sstex : screenSizeTextures_s) {
      sstex->resize();
    }

    // Reset this flag, which is set to true if VkAquireNextImageKHR ever returns VK_ERROR_OUT_OF_DATE_KHR.
    swapchainNeedsRebuild_s = false;
  }
  // The swapchain hasn't been resized, but we'll check for 0 just in case.
  else if (swapchainExtent_s.width == 0 || swapchainExtent_s.height == 0)
    return false;

  // The swapchain hasn't been resized and its size is non-zero, so we're good to go.
  return true;
}

void hlgl::_impl::registerScreenSizeTexture(hlgl::Texture* texture) {
  screenSizeTextures_s.insert(texture);
}

void hlgl::_impl::unregisterScreenSizeTexture(hlgl::Texture* texture) {
  screenSizeTextures_s.erase(texture);
}
