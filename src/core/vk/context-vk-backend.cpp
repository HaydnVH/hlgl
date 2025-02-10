#define VMA_IMPLEMENTATION
#define VOLK_IMPLEMENTATION
#include "vk-includes.h"
#include "vk-debug.h"
#include "vk-translate.h"
#include <hlgl/core/context.h>

#ifdef HLGL_WINDOW_LIBRARY_GLFW
#include <GLFW/glfw3.h>
#endif

#ifdef HLGL_INCLUDE_IMGUI
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#ifdef HLGL_WINDOW_LIBRARY_GLFW
#include <backends/imgui_impl_glfw.h>
#endif
#endif

#include <set>
#include <utility>
#include <vector>
#include <fmt/format.h>


namespace {

bool isLayerSupported(const std::vector<VkLayerProperties>& supportedLayers, std::string_view desiredLayer) {
  for (const auto& supportedLayer : supportedLayers) {
    if (desiredLayer == std::string_view(supportedLayer.layerName))
      return true;
  }
  return false;
}

bool isExtensionSupported(const std::vector<VkExtensionProperties>& supportedExtensions, std::string_view desiredExtension) {
  for (const auto& supportedExtension : supportedExtensions) {
    if (desiredExtension == std::string_view(supportedExtension.extensionName))
      return true;
  }
  return false;
}

} // namespace <anon>

bool hlgl::Context::initInstance(const VkApplicationInfo& appInfo, Features preferredFeatures, Features requiredFeatures) {

  if (volkInitialize() != VK_SUCCESS) {
    debugPrint(DebugSeverity::Error, "Failed to initialize volk; no vulkan-capable drivers installed?");
    return false;
  }

  std::vector<const char*> requestedLayers {};
  uint32_t layerCount {0};
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> layerProperties(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

  bool bail {false};
  for (auto requiredLayer : requestedLayers) {
    if (!isLayerSupported(layerProperties, requiredLayer)) {
      debugPrint(DebugSeverity::Error, fmt::format("Failed to find required Vulkan layer: {}", requiredLayer));
      bail = true;
    }
  }
  if (bail)
    return false;

  debugPrint(DebugSeverity::Debug, fmt::format("Found {} required Vulkan layer(s):", requestedLayers.size()));
  for (auto layer : requestedLayers) { debugPrint(DebugSeverity::Debug, fmt::format("  - {}", layer)); }
  size_t requiredLayerCount {requestedLayers.size()};

  // We want to know if validation layers are available.
  constexpr const char* validationLayerName {"VK_LAYER_KHRONOS_validation"};
  if (isLayerSupported(layerProperties, validationLayerName))
    gpu_.supportedFeatures |= Feature::Validation;

  // If the user set validation as a required feature and it's not available, print an error and bail out.
  if ((requiredFeatures & Feature::Validation) && !(gpu_.supportedFeatures & Feature::Validation)) {
    debugPrint(DebugSeverity::Error,
      "Validation layers are required, but not supported.  You may need to install the Vulkan SDK or install the layers manually.");
    return false;
  }

  // If validation layers are required or preferred, add them to the list so they can be enabled.
  if (preferredFeatures & Feature::Validation) {
    requestedLayers.push_back(validationLayerName);
    gpu_.enabledFeatures |= Feature::Validation;
  }

  debugPrint(DebugSeverity::Debug, fmt::format("Found {} optional Vulkan layer(s):", requestedLayers.size() - requiredLayerCount));
  for (auto layer {requestedLayers.begin()+requiredLayerCount}; layer != requestedLayers.end(); ++layer) {
    debugPrint(DebugSeverity::Debug, fmt::format("  - {}", *layer));
  }

  // Start this list off with the list of instance extensions which are absolutely required.
  std::vector<const char*> requestedExtensions {};

  // Add platform-specific extensions to the list.
#if defined HLGL_WINDOW_LIBRARY_GLFW
  uint32_t glfwExtensionCount {0};
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  for (uint32_t i {0}; i < glfwExtensionCount; ++i) { requestedExtensions.push_back(glfwExtensions[i]); }
#endif

  uint32_t extensionCount {0};
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> extensionProperties(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());

  // Make sure the required instance extensions are available and bail out if any aren't.
  bail = false;
  for (auto requiredExtension : requestedExtensions) {
    if (!isExtensionSupported(extensionProperties, requiredExtension)) {
      debugPrint(DebugSeverity::Error, fmt::format("Failed to find required Vulkan instance extension: {}", requiredExtension));
      bail = true;
    }
  }
  if (bail)
    return {};

  debugPrint(DebugSeverity::Debug, fmt::format("Found {} required Vulkan instance extension(s):", requestedExtensions.size()));
  for (auto& ext: requestedExtensions) { debugPrint(DebugSeverity::Debug, fmt::format("  - {}", ext)); }
  size_t requiredExtCount = requestedExtensions.size();

  // Always enable the color space extension if present, since HDR can be toggled at runtime.
  if (isExtensionSupported(extensionProperties, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME)) {
    requestedExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
  }

  // Enable debug extensions only if validation is requested.
  if (gpu_.enabledFeatures & Feature::Validation) {
    if (isExtensionSupported(extensionProperties, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
      requestedExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    else {
      debugPrint(DebugSeverity::Warning, fmt::format("Validation requested but extension '{}' isn't present.", VK_EXT_DEBUG_UTILS_EXTENSION_NAME));
      gpu_.enabledFeatures &= ~Feature::Validation;
      gpu_.supportedFeatures &= ~Feature::Validation;
    }
  }

  debugPrint(DebugSeverity::Debug, fmt::format("Found {} optional Vulkan instance extension(s):", requestedExtensions.size() - requiredExtCount));
  for (auto it{requestedExtensions.begin()+requiredExtCount}; it != requestedExtensions.end(); ++it) {
    debugPrint(DebugSeverity::Debug, fmt::format("  - {}", *it));
  }

  VkInstanceCreateInfo ci {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .flags = 0,
    .pApplicationInfo = &appInfo,
    .enabledLayerCount = (uint32_t)requestedLayers.size(),
    .ppEnabledLayerNames = requestedLayers.data(),
    .enabledExtensionCount = (uint32_t)requestedExtensions.size(),
    .ppEnabledExtensionNames = requestedExtensions.data()
  };

  VkValidationFeatureEnableEXT enabledValidationFeatures[] = {
    VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT };
  VkValidationFeaturesEXT vf {
    .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
    .enabledValidationFeatureCount = std::size(enabledValidationFeatures),
    .pEnabledValidationFeatures = enabledValidationFeatures };
  if (gpu_.enabledFeatures & Feature::Validation) {
    ci.pNext = &vf;
  }

  if (!VKCHECK(vkCreateInstance(&ci, nullptr, &instance_)) || !instance_) {
    debugPrint(DebugSeverity::Error, "Failed to create Vulkan instance.");
    return false;
  }

  volkLoadInstance(instance_);
  debugPrint(DebugSeverity::Debug, "Created Vulkan instance.");
  return true;
}


VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity,
  VkDebugUtilsMessageTypeFlagsEXT type,
  const VkDebugUtilsMessengerCallbackDataEXT* data,
  void* userData)
{
  using namespace hlgl;
  switch (severity) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    hlgl::debugPrint(hlgl::DebugSeverity::Trace, fmt::format("[VK] {}", data->pMessage));
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    hlgl::debugPrint(hlgl::DebugSeverity::Info, fmt::format("[VK] {}", data->pMessage));
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    hlgl::debugPrint(hlgl::DebugSeverity::Warning, fmt::format("[VK] {}", data->pMessage));
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    hlgl::debugPrint(hlgl::DebugSeverity::Error, fmt::format("[VK] {}", data->pMessage));
    break;
  default:
    break;
  }
  return VK_FALSE;
}


bool hlgl::Context::initDebug() {
  if (!instance_) {
    debugPrint(DebugSeverity::Error, "Instance must be initialized before Debug Messenger.");
    return false;
  }

  VkDebugUtilsMessengerCreateInfoEXT ci = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType =     VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = debugCallback
  };

  if (!VKCHECK(vkCreateDebugUtilsMessengerEXT(instance_, &ci, nullptr, &debug_)) || !debug_) {
    debugPrint(DebugSeverity::Error, "Failed to create Vulkan debug messenger.");
    return false;
  }

  debugPrint(DebugSeverity::Debug, "Created Vulkan debug messenger.");
  return true;
}


bool hlgl::Context::initSurface(WindowHandle window)
{
  if (!instance_) {
    debugPrint(DebugSeverity::Error, "Instance must be initialized before Surface.");
    return false;
  }

#ifdef HLGL_WINDOW_LIBRARY_GLFW
  if (!VKCHECK(glfwCreateWindowSurface(instance_, window, nullptr, &surface_)) || !surface_) {
    debugPrint(DebugSeverity::Error, "Failed to create Vulkan window surface for GLFW.");
    return false;
  }
#endif

  debugPrint(DebugSeverity::Debug, "Created Vulkan window surface.");
  return true;
}

// Get the most appropriate queue families for each queue type.
void getQueueFamilyIndices(
  VkPhysicalDevice physicalDevice,
  VkSurfaceKHR surface,
  uint32_t& outGraphics,
  uint32_t& outPresent,
  uint32_t& outCompute,
  uint32_t& outTransfer)
{
  outGraphics = UINT32_MAX;
  outPresent = UINT32_MAX;
  outCompute = UINT32_MAX;
  outTransfer = UINT32_MAX;
  uint32_t minTransferScore {UINT32_MAX};

  uint32_t familyCount {0};
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
  std::vector<VkQueueFamilyProperties> families(familyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());

  for (uint32_t i{0}; i < (uint32_t)families.size(); ++i) {
    uint32_t curTransferScore {0};
    if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
    { outGraphics = i; ++curTransferScore; }
    VkBool32 presentSupport {false};
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
    if (presentSupport)
    { outPresent = i; ++curTransferScore; }
    if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
    { outCompute = i; ++curTransferScore; }
    if ((families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && (curTransferScore < minTransferScore))
    { outTransfer = i; minTransferScore = curTransferScore; }
  }
}

const std::array requiredDeviceExtensions {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
  VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
  VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
  VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
};

using GpuPair = std::pair<hlgl::GpuProperties, VkPhysicalDevice>;

bool hlgl::Context::pickPhysicalDevice(
  const char* preferredPhysicalDevice,
  Features preferredFeatures,
  Features requiredFeatures)
{
  if (!instance_ || !surface_) {
    debugPrint(DebugSeverity::Error, "Instance and Surface must be initialized before picking a physical device.");
    return false;
  }

  // Get the list of installed physical devices and filter out the inappropriate ones.
  uint32_t availableDeviceCount {0};
  vkEnumeratePhysicalDevices(instance_, &availableDeviceCount, nullptr);
  std::vector<VkPhysicalDevice> availableDevices(availableDeviceCount);
  vkEnumeratePhysicalDevices(instance_, &availableDeviceCount, availableDevices.data());

  std::vector<GpuPair> appropriateDevices;
  for (auto physicalDevice : availableDevices) {
    VkPhysicalDeviceProperties pdProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &pdProperties);
    debugPrint(DebugSeverity::Trace, fmt::format("Found physical device '{}', checking properties...",
      pdProperties.deviceName));

    // Make sure the neccessary queue families are supported.
    uint32_t graphicsFamily, presentFamily, computeFamily, transferFamily;
    getQueueFamilyIndices(physicalDevice, surface_, graphicsFamily, presentFamily, computeFamily, transferFamily);
    if (graphicsFamily == UINT32_MAX ||
      presentFamily == UINT32_MAX ||
      computeFamily == UINT32_MAX ||
      transferFamily == UINT32_MAX)
    {
      debugPrint(DebugSeverity::Trace, "  ...required queue familes not supported, skipping.");
      continue;
    }

    // Get the list of device extensions supported by this physical device.
    uint32_t extensionCount {0};
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensionProperties.data());

    // Make sure the neccessary device extensions are supported.
    bool skip {false};
    for (auto requiredExtension : requiredDeviceExtensions) {
      if (!isExtensionSupported(extensionProperties, requiredExtension)) {
        debugPrint(DebugSeverity::Trace, fmt::format("  ...required device extension '{}' not supported, skipping.", requiredExtension));
        skip = true;
      }
    }
    if (skip) continue;

    // Assemble the GpuProperties struct.
    // The "gpu" passed in will have supportedFeatures set from instance creation,
    // but all other fields defaulted, so use it here to initialize.
    GpuProperties properties = gpu_;
    properties.sName = pdProperties.deviceName;

    VkPhysicalDeviceMemoryProperties memory;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memory);
    for (uint32_t i {0}; i < memory.memoryHeapCount; ++i) {
      if (memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        properties.iDeviceMemory = std::max(properties.iDeviceMemory, (uint64_t)memory.memoryHeaps[i].size);
    }
    switch (pdProperties.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_CPU:             properties.eType = GpuType::Cpu; break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:     properties.eType = GpuType::Virtual; break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:  properties.eType = GpuType::Integrated; break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:    properties.eType = GpuType::Discrete; break;
    }

    // Get as many supported features as we can.

    if (isExtensionSupported(extensionProperties, VK_EXT_SHADER_OBJECT_EXTENSION_NAME)) {
      properties.supportedFeatures |= Feature::ShaderObjects;
    }

    // Check for buffer device address
    if (isExtensionSupported(extensionProperties, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
      properties.supportedFeatures |= Feature::BufferDeviceAddress;
    } else if (requiredFeatures & Feature::BufferDeviceAddress) {
      debugPrint(DebugSeverity::Trace, fmt::format("  ...required device extension '{}' not supported, skipping.", VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME));
      continue;
    }

    if (isExtensionSupported(extensionProperties, VK_EXT_MESH_SHADER_EXTENSION_NAME) ||
      isExtensionSupported(extensionProperties, VK_NV_MESH_SHADER_EXTENSION_NAME))
    {
      properties.supportedFeatures |= Feature::MeshShading;
    }
    else if (requiredFeatures & Feature::MeshShading) {
      debugPrint(DebugSeverity::Trace, fmt::format("  ...required device extension '{}' not supported, skipping.", VK_EXT_MESH_SHADER_EXTENSION_NAME));
      continue;
    }

    if (isExtensionSupported(extensionProperties, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) ||
      isExtensionSupported(extensionProperties, VK_NV_RAY_TRACING_EXTENSION_NAME))
    {
      properties.supportedFeatures |= Feature::Raytracing;
    }
    else if (requiredFeatures & Feature::Raytracing) {
      debugPrint(DebugSeverity::Trace, fmt::format("  ...required device extension '{}' not supported, skipping.", VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME));
      continue;
    }

    // Get surface formats and check for HDR support.
    uint32_t surfaceFormatCount {0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface_, &surfaceFormatCount, nullptr);
    if (surfaceFormatCount == 0) {
      debugPrint(DebugSeverity::Trace, "  ...no surface formats available, skipping.");
      continue;
    }
    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface_, &surfaceFormatCount, surfaceFormats.data());
    for (auto& format : surfaceFormats) {
      if (format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
        format.format == VK_FORMAT_R16G16B16A16_SFLOAT)
        properties.supportedFeatures |= Feature::DisplayHdr;
    }
    if (!(properties.supportedFeatures & Feature::DisplayHdr) && (requiredFeatures & Feature::DisplayHdr)) {
      debugPrint(DebugSeverity::Trace, "  ...required feature 'Hdr' not supported, skipping.");
      continue;
    }

    // Get surface present modes.
    uint32_t presentModeCount {0};
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface_, &presentModeCount, nullptr);
    if (presentModeCount == 0) {
      debugPrint(DebugSeverity::Trace, "  ...no surface present modes available, skipping.");
      continue;
    }
    // TODO: check for vsync support (or not-vsync support, as it were).

    // TODO: Check for bindless texturing
    
    // TODO: Check for sampler minmax

    // Record the driver version.
    properties.driverVersion = {
      VK_VERSION_MAJOR(pdProperties.driverVersion),
      VK_VERSION_MINOR(pdProperties.driverVersion),
      VK_VERSION_PATCH(pdProperties.driverVersion)};

    // Record the API version.
    properties.apiVersion = {
      VK_VERSION_MAJOR(pdProperties.apiVersion),
      VK_VERSION_MINOR(pdProperties.apiVersion),
      VK_VERSION_PATCH(pdProperties.apiVersion)};

    // Record the Vendor ID.
    switch (pdProperties.vendorID) {
    case 0x1002: properties.eVendor = Vendor::AMD; break;
    case 0x1010: properties.eVendor = Vendor::ImgTec; break;
    case 0x10de: properties.eVendor = Vendor::NVIDIA; break;
    case 0x1385: properties.eVendor = Vendor::ARM; break;
    case 0x5143: properties.eVendor = Vendor::Qualcomm; break;
    case 0x8086: properties.eVendor = Vendor::INTEL; break;
    default: properties.eVendor = Vendor::Other; break;
    }

    debugPrint(DebugSeverity::Trace, "  ...requirements are met, adding to list.");
    appropriateDevices.push_back({properties, physicalDevice});
  }

  if (appropriateDevices.size() == 0) {
    debugPrint(DebugSeverity::Error, "Failed to find any appropriate physical devices.");
    return false;
  }

  // If the user requests a particular physical device, look for it here.
  GpuPair chosenDevice {{}, nullptr};
  std::string_view preferredPhysicalDeviceView((preferredPhysicalDevice) ? preferredPhysicalDevice : "");
  if (preferredPhysicalDeviceView != "") {
    auto found = std::find_if(appropriateDevices.begin(), appropriateDevices.end(),
      [&](const GpuPair& pair){ return (preferredPhysicalDeviceView == pair.first.sName); });
    if (found == appropriateDevices.end()) {
      debugPrint(DebugSeverity::Warning, fmt::format("Couldn't find preferred physical device '{}', will choose the most appropriate instead.",
        preferredPhysicalDeviceView));
    }
    else
      chosenDevice = *found;
  }

  // If we didn't find a preferred physical device, choose the most appropriate to use instead.
  if (chosenDevice.second == nullptr) {
    std::sort(appropriateDevices.begin(), appropriateDevices.end(), [&](const GpuPair& a, const GpuPair& b)
    {
      // First priority is support for preferred features.
      int aPreferredFeatureCount = a.first.supportedFeatures.bitsInCommon(preferredFeatures);
      int bPreferredFeatureCount = b.first.supportedFeatures.bitsInCommon(preferredFeatures);
      if (aPreferredFeatureCount != bPreferredFeatureCount)
        return (aPreferredFeatureCount < bPreferredFeatureCount);

      // If the supported feature count is the same, then compare the device type.  Discrete GPU is ideal.
      if (a.first.eType != b.first.eType)
        return ((int)a.first.eType < (int)b.first.eType);

      // If the device type is also the same, compare the amount of available device memory.
      return (a.first.iDeviceMemory < b.first.iDeviceMemory);
    });
    chosenDevice = appropriateDevices.back();
  }
  gpu_ = chosenDevice.first;

  debugPrint(DebugSeverity::Info, fmt::format("Using {} ({}) with {} bytes of device memory.",
    gpu_.sName, gpu_.eType, gpu_.iDeviceMemory));
  debugPrint(DebugSeverity::Info, fmt::format("Driver version {}.{}.{}, Vulkan API version {}.{}.{}",
    gpu_.driverVersion.major, gpu_.driverVersion.minor, gpu_.driverVersion.patch,
    gpu_.apiVersion.major, gpu_.apiVersion.minor, gpu_.apiVersion.patch));

  physicalDevice_ = chosenDevice.second;
  return true;
}


bool hlgl::Context::initDevice(
  Features preferredFeatures,
  Features requiredFeatures)
{
  if (!physicalDevice_ || !surface_) {
    debugPrint(DebugSeverity::Error, "Physical device must be chosen before initializing logical device.");
    return false;
  }

  debugPrint(DebugSeverity::Debug, fmt::format("Found {} required Vulkan device extension(s):", requiredDeviceExtensions.size()));
  for (auto& ext : requiredDeviceExtensions) { debugPrint(DebugSeverity::Debug, fmt::format("  - {}", ext)); }

  // Assemble the list of extensions to request.
  std::vector<const char*> extensions(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());

  if ((gpu_.supportedFeatures & Feature::BufferDeviceAddress && (preferredFeatures & Feature::BufferDeviceAddress))) {
    extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    gpu_.enabledFeatures |= Feature::BufferDeviceAddress;
  }


  if ((gpu_.supportedFeatures & Feature::MeshShading) && (preferredFeatures & Feature::MeshShading)) {
    extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    gpu_.enabledFeatures |= Feature::MeshShading;
  }

  if ((gpu_.supportedFeatures & Feature::Raytracing) && (preferredFeatures & Feature::Raytracing)) {
    extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    gpu_.enabledFeatures |= Feature::Raytracing;
  }

  debugPrint(DebugSeverity::Debug, fmt::format("Found {} optional Vulkan device extension(s):", extensions.size() - requiredDeviceExtensions.size()));
  for (auto it{extensions.begin() + requiredDeviceExtensions.size()}; it != extensions.end(); ++it) {
    debugPrint(DebugSeverity::Debug, fmt::format("  - {}", *it));
  }

  // Assemble queue family indices.
  uint32_t graphicsFamily, presentFamily, computeFamily, transferFamily;
  getQueueFamilyIndices(physicalDevice_, surface_, graphicsFamily, presentFamily, computeFamily, transferFamily);
  std::set<uint32_t> uniqueIndices {graphicsFamily, presentFamily, computeFamily, transferFamily};

  // Build the queue create infos.
  float queuePriority {1.0f};
  std::vector<VkDeviceQueueCreateInfo> qci;
  for (auto& index : uniqueIndices) {
    qci.push_back({
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = index,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority
      });
  }

  // Enable desired features.
  // TODO: Test for the availability of these during physical device selection.

  void *pNext {nullptr};

  VkPhysicalDeviceMeshShaderFeaturesEXT msf {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
    .pNext = pNext,
    .taskShader = true,
    .meshShader = true
  };
  if (gpu_.enabledFeatures & Feature::MeshShading)
    pNext = &msf;

  VkPhysicalDeviceDynamicRenderingFeatures drf {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
    .pNext = pNext,
    .dynamicRendering = true,
  };
  pNext = &drf;

  VkPhysicalDeviceSynchronization2Features sync {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
    .pNext = pNext,
    .synchronization2 = true,
  };
  pNext = &sync;

  VkPhysicalDeviceVulkan12Features df12 {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .pNext = pNext,
    .drawIndirectCount = true,
    .storageBuffer8BitAccess = true,
    .uniformAndStorageBuffer8BitAccess = true,
    .storagePushConstant8 = true,
    .shaderFloat16 = true,
    .shaderInt8 = true,
    .samplerFilterMinmax = true,
    .bufferDeviceAddress = true,
  };
  pNext = &df12;

  VkPhysicalDeviceVulkan11Features df11 {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    .pNext = pNext,
    .storageBuffer16BitAccess = true,
    .shaderDrawParameters = true,
  };
  pNext = &df11;

  VkPhysicalDeviceFeatures2 df2 {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    .pNext = pNext,
    .features = {
      .samplerAnisotropy = true,
      .pipelineStatisticsQuery = true,
      .shaderInt16 = true,
  }
  };
  pNext = &df2;

  VkDeviceCreateInfo ci {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = pNext,
    .queueCreateInfoCount = (uint32_t)qci.size(),
    .pQueueCreateInfos = qci.data(),
    .enabledExtensionCount = (uint32_t)extensions.size(),
    .ppEnabledExtensionNames = extensions.data()
  };

  // Finally create the logical device.
  if (!VKCHECK(vkCreateDevice(physicalDevice_, &ci, nullptr, &device_)) || !device_) {
    debugPrint(DebugSeverity::Error, "Failed to create Vulkan logical device.");
    return false;
  }

  // Get device function pointers to bypass Vulkan's loader dispatch.
  volkLoadDevice(device_);

  // Now that we have a Device, we can go back and add debug names for existing objects.
  if (gpu_.enabledFeatures & Feature::Validation) {
    VkDebugUtilsObjectNameInfoEXT info { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };

    info.objectType = VK_OBJECT_TYPE_INSTANCE;
    info.objectHandle = (uint64_t)instance_;
    info.pObjectName = "context.instance";
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_, &info)))
      return false;

    info.objectType = VK_OBJECT_TYPE_SURFACE_KHR;
    info.objectHandle = (uint64_t)surface_;
    info.pObjectName = "context.surface";
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_, &info)))
      return false;

    info.objectType = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
    info.objectHandle = (uint64_t)physicalDevice_;
    info.pObjectName = "context.physicalDevice";
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_, &info)))
      return false;

    info.objectType = VK_OBJECT_TYPE_DEVICE;
    info.objectHandle = (uint64_t)device_;
    info.pObjectName = "context.device";
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_, &info)))
      return false;
  }

  debugPrint(DebugSeverity::Debug, "Created Vulkan logical device.");
  return true;
}


bool hlgl::Context::initQueues()
{
  if (!device_ || !physicalDevice_ || !surface_) {
    debugPrint(DebugSeverity::Error, "Logical device must be initialized before queues.");
    return false;
  }

  getQueueFamilyIndices(physicalDevice_, surface_, graphicsQueueFamily_, presentQueueFamily_, computeQueueFamily_, transferQueueFamily_);
  if (graphicsQueueFamily_ == UINT32_MAX || presentQueueFamily_ == UINT32_MAX || computeQueueFamily_ == UINT32_MAX || transferQueueFamily_ == UINT32_MAX) {
    debugPrint(DebugSeverity::Error, "Failed to get queue families.");
    return false;
  }

  vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
  vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);
  vkGetDeviceQueue(device_, computeQueueFamily_, 0, &computeQueue_);
  vkGetDeviceQueue(device_, transferQueueFamily_, 0, &transferQueue_);

  if (gpu_.enabledFeatures & Feature::Validation) {
    std::set<VkQueue> queueSet {graphicsQueue_, presentQueue_, computeQueue_, transferQueue_};
    for (VkQueue queue : queueSet) {
      std::string queueName = "context.queues.";
      if (queue == graphicsQueue_ && queue == presentQueue_ && queue == computeQueue_ && queue == transferQueue_) queueName += "all";
      else {
        if (queue == graphicsQueue_) { queueName += "graphics"; }
        if (queue == presentQueue_) { if (queueName == "context.queues.") queueName = "present"; else queueName += "|present"; }
        if (queue == computeQueue_) { if (queueName == "context.queues.") queueName = "compute"; else queueName += "|compute"; }
        if (queue == transferQueue_) { if (queueName == "context.queues.") queueName = "transfer"; else queueName += "|transfer"; } }

      VkDebugUtilsObjectNameInfoEXT info {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_QUEUE,
        .objectHandle = (uint64_t)queue,
        .pObjectName = queueName.c_str() };
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_, &info)))
        return false;
    }
  }

  debugPrint(DebugSeverity::Debug,
    fmt::format("Using Vulkan device queues with family indices: {}(graphics), {}(present), {}(compute), {}(transfer)",
      graphicsQueueFamily_, presentQueueFamily_, computeQueueFamily_, transferQueueFamily_));
  return true;
}


bool hlgl::Context::initCmdPool()
{
  if (!device_ || graphicsQueueFamily_ == UINT32_MAX) {
    debugPrint(DebugSeverity::Error, "Device must be initialized before command pool.");
    return false;
  }

  VkCommandPoolCreateInfo ci {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = graphicsQueueFamily_ };

  if (!VKCHECK(vkCreateCommandPool(device_, &ci, nullptr, &cmdPool_)) || !cmdPool_) {
    debugPrint(DebugSeverity::Error, "Failed to create Vulkan command pool.");
    return false;
  }

  if (gpu_.enabledFeatures & Feature::Validation) {
    VkDebugUtilsObjectNameInfoEXT info {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = VK_OBJECT_TYPE_COMMAND_POOL,
      .objectHandle = (uint64_t)cmdPool_,
      .pObjectName = "context.cmdPool" };
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_, &info)))
      return false;
  }

  debugPrint(DebugSeverity::Debug, "Created Vulkan command pool.");
  return true;
}

bool hlgl::Context::initDescPool() {
  VkDescriptorPoolSize poolSizes[] = {
    {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
    {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
    {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
    {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
  };

  VkDescriptorPoolCreateInfo ci {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    .maxSets = 1000,
    .poolSizeCount = (uint32_t)std::size(poolSizes),
    .pPoolSizes = poolSizes };
  if (!VKCHECK(vkCreateDescriptorPool(device_, &ci, nullptr, &descPool_)))
    return false;

  debugPrint(DebugSeverity::Debug, "Created Vulkan descriptor pool.");
  return true;
}

bool hlgl::Context::resizeSwapchain() {
  if (!device_ || !physicalDevice_ || !surface_) {
    debugPrint(DebugSeverity::Error, "Device must be initialized before building the swapchain.");
    return false;
  }

  VkSurfaceCapabilitiesKHR caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);

  // Get the number of images that the swapchain should contain.  We want at least 2.
  uint32_t imgCount = (caps.maxImageCount > caps.minImageCount) ?
    std::clamp<uint32_t>(2, caps.minImageCount, caps.maxImageCount) :
    std::max<uint32_t>(2, caps.minImageCount);

  // Get the surface format.
  VkFormat preferredFormat = displayHdr_ ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : VK_FORMAT_R8G8B8A8_UNORM;
  uint32_t formatCount {0};
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());

  VkSurfaceFormatKHR surfaceFormat {formats.front()};
  for (auto& format : formats) {
    if (format.format == preferredFormat)
      surfaceFormat = format;
  }
  // Save the format and tell the Gpu properties whether HDR is enabled.
  swapchainFormat_ = surfaceFormat.format;
  if (displayHdr_)
    gpu_.enabledFeatures |= Feature::DisplayHdr;
  else
    gpu_.enabledFeatures &= ~Feature::DisplayHdr;

  // Get the swapchain extent.
  if (caps.currentExtent.width != UINT32_MAX && caps.currentExtent.height != UINT32_MAX)
    swapchainExtent_ = caps.currentExtent;
  else {
    swapchainExtent_.width = std::clamp<uint32_t>(displayWidth_, caps.minImageExtent.width, caps.maxImageExtent.width);
    swapchainExtent_.height = std::clamp<uint32_t>(displayHeight_, caps.minImageExtent.height, caps.maxImageExtent.height);
  }

  // Get the sharing mode and resolve queue family indices.
  VkSharingMode sharing;
  std::vector<uint32_t> queueFamilyIndices {};
  if (graphicsQueueFamily_ == presentQueueFamily_) {
    sharing = VK_SHARING_MODE_EXCLUSIVE;
  }
  else {
    sharing = VK_SHARING_MODE_CONCURRENT;
    queueFamilyIndices = {graphicsQueueFamily_, presentQueueFamily_};
  }

  // Get the present mode.
  VkPresentModeKHR preferredPresentMode = displayVsync_ ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
  VkPresentModeKHR presentMode {VK_PRESENT_MODE_FIFO_KHR};
  uint32_t presentModeCount {0};
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, nullptr);
  std::vector<VkPresentModeKHR> presentModes(presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, presentModes.data());
  if (std::find(presentModes.begin(), presentModes.end(), preferredPresentMode) != presentModes.end())
    presentMode = preferredPresentMode;
  // Save the present mode and tell the Gpu properties whether vsync is enabled;
  if (displayVsync_)
    gpu_.enabledFeatures |= Feature::DisplayVsync;
  else
    gpu_.enabledFeatures &= ~Feature::DisplayVsync;

  VkSwapchainKHR newSwapchain;
  VkSwapchainCreateInfoKHR ci {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = surface_,
    .minImageCount = imgCount,
    .imageFormat = surfaceFormat.format,
    .imageColorSpace = surfaceFormat.colorSpace,
    .imageExtent = swapchainExtent_,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    .imageSharingMode = sharing,
    .queueFamilyIndexCount = (uint32_t)queueFamilyIndices.size(),
    .pQueueFamilyIndices = queueFamilyIndices.data(),
    .preTransform = caps.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = presentMode,
    .clipped = true,
    .oldSwapchain = swapchain_ };
  if (!VKCHECK(vkCreateSwapchainKHR(device_, &ci, nullptr, &newSwapchain)))
    return false;

  // Destroy the old swapchain.
  swapchainTextures_.clear();
  if (swapchain_)
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);

  // Save the new swapchain.
  swapchain_ = newSwapchain;
  if (swapchain_ == nullptr) {
    debugPrint(DebugSeverity::Error, "Failed to create swapchain.");
    return false;
  }

  debugPrint(DebugSeverity::Trace, fmt::format("Created swapchain with dimensions {} x {}", swapchainExtent_.width, swapchainExtent_.height));

  if (gpu_.enabledFeatures & Feature::Validation) {
    VkDebugUtilsObjectNameInfoEXT info {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
      .objectHandle = (uint64_t)swapchain_,
      .pObjectName = "context.swapchain" };
    if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_, &info)))
      return false;
  }

  displayWidth_ = swapchainExtent_.width;
  displayHeight_ = swapchainExtent_.height;

  // Get the swapchain images.
  uint32_t imageCount {0};
  vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
  std::vector<VkImage> images(imageCount);
  vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, images.data());

  // Use the images to create textures, which handle image views and such.
  swapchainTextures_.reserve(images.size());
  for (size_t i {0}; i < images.size(); ++i) {
    swapchainTextures_.emplace_back(*this, TextureParams{
      .iWidth = swapchainExtent_.width,
      .iHeight = swapchainExtent_.height,
      .eFormat = translate(surfaceFormat.format),
      .sDebugName = fmt::format("context.swapchain[{}]", i).c_str(),
      .pExistingImage = images[i]});
  }
  return true;
}


bool hlgl::Context::initFrames()
{
  if (!device_ || !cmdPool_) {
    debugPrint(DebugSeverity::Error, "Device and Command Pool must be initialised before synchronization frames.");
    return false;
  }

  for (size_t i {0}; i < frames_.size(); ++i) {
    VkCommandBufferAllocateInfo ai {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = cmdPool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1 };
    if (!VKCHECK(vkAllocateCommandBuffers(device_, &ai, &frames_[i].cmd)))
      return false;

    VkSemaphoreCreateInfo sci { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    if (!VKCHECK(vkCreateSemaphore(device_, &sci, nullptr, &frames_[i].imageAvailable)))
      return false;
    if (!VKCHECK(vkCreateSemaphore(device_, &sci, nullptr, &frames_[i].renderFinished)))
      return false;

    VkFenceCreateInfo fci { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    if (!VKCHECK(vkCreateFence(device_, &fci, nullptr, &frames_[i].fence)))
      return false;

    if (gpu_.enabledFeatures & Feature::Validation) {
      VkDebugUtilsObjectNameInfoEXT info { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
      info.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
      info.objectHandle = (uint64_t)frames_[i].cmd;
      auto name = fmt::format("context.frames[{}].cmd", i);
      info.pObjectName = name.c_str();
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_, &info)))
        return false;

      info.objectType = VK_OBJECT_TYPE_SEMAPHORE;
      info.objectHandle = (uint64_t)frames_[i].imageAvailable;
      name = fmt::format("context.frames[{}].imageAvailable", i);
      info.pObjectName = name.c_str();
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_, &info)))
        return false;

      info.objectType = VK_OBJECT_TYPE_SEMAPHORE;
      info.objectHandle = (uint64_t)frames_[i].renderFinished;
      name = fmt::format("context.frames[{}].renderFinished", i);
      info.pObjectName = name.c_str();
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_, &info)))
        return false;

      info.objectType = VK_OBJECT_TYPE_FENCE;
      info.objectHandle = (uint64_t)frames_[i].fence;
      name = fmt::format("context.frames[{}].fence", i);
      info.pObjectName = name.c_str();
      if (!VKCHECK(vkSetDebugUtilsObjectNameEXT(device_, &info)))
        return false;
    }
  }

  debugPrint(DebugSeverity::Debug, "Created command buffers and synchronization primitives for frames in flight.");
  return true;
}

bool hlgl::Context::initAllocator() {
  using namespace hlgl;
  if (!physicalDevice_ || !device_ || !instance_) {
    debugPrint(DebugSeverity::Error, "Device must be initialized before allocator.");
    return false;
  }

  VmaVulkanFunctions volkFunctions =
  {
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
    .physicalDevice = physicalDevice_,
    .device = device_,
    .pVulkanFunctions = &volkFunctions,
    .instance = instance_,
  };
  if (!VKCHECK(vmaCreateAllocator(&ci, &allocator_)))
    return false;

  debugPrint(DebugSeverity::Debug, "Created VMA allocator.");
  return true;
}


bool hlgl::Context::initImGui(WindowHandle window) {
#ifdef HLGL_INCLUDE_IMGUI
  ImGui::CreateContext();
#ifdef HLGL_WINDOW_LIBRARY_GLFW
  ImGui_ImplGlfw_InitForVulkan(window, true);
#endif
  ImGui_ImplVulkan_InitInfo ii {
    .Instance = instance_,
    .PhysicalDevice = physicalDevice_,
    .Device = device_,
    .QueueFamily = graphicsQueueFamily_,
    .Queue = graphicsQueue_,
    .DescriptorPool = descPool_,
    .MinImageCount = 3,
    .ImageCount = 3,
    .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
    .UseDynamicRendering = true,
    .PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &swapchainFormat_ }
  };
  ImGui_ImplVulkan_Init(&ii);
  ImGui_ImplVulkan_CreateFontsTexture();
  gpu_.enabledFeatures |= Feature::Imgui;
  debugPrint(DebugSeverity::Debug, "Initialized ImGui for Vulkan.");
#endif
  return true;
}


void hlgl::Context::destroyBackend() {
  if (device_) {
  #ifdef HLGL_INCLUDE_IMGUI
    if (gpu_.enabledFeatures & Feature::Imgui) {
      ImGui_ImplVulkan_Shutdown();
    #ifdef HLGL_WINDOW_LIBRARY_GLFW
      ImGui_ImplGlfw_Shutdown();
    #endif
      ImGui::DestroyContext();
    }
  #endif
    if (allocator_) { vmaDestroyAllocator(allocator_); allocator_ = nullptr; }
    for (auto& frame : frames_) {
      if (frame.fence) { vkDestroyFence(device_, frame.fence, nullptr); frame.fence = nullptr; }
      if (frame.renderFinished) { vkDestroySemaphore(device_, frame.renderFinished, nullptr); frame.renderFinished = nullptr; }
      if (frame.imageAvailable) { vkDestroySemaphore(device_, frame.imageAvailable, nullptr); frame.imageAvailable = nullptr; }
      if (frame.cmd && cmdPool_) { vkFreeCommandBuffers(device_, cmdPool_, 1, &frame.cmd); frame.cmd = nullptr; }
    }
    swapchainTextures_.clear();
    // The swapchain textures have been added to the deletion queue after we already flushed it, so flush it again here.
    flushAllDelQueues();
    if (swapchain_) { vkDestroySwapchainKHR(device_, swapchain_, nullptr); swapchain_ = nullptr; }
    if (descPool_) { vkDestroyDescriptorPool(device_, descPool_, nullptr); descPool_ = nullptr; }
    if (cmdPool_) { vkDestroyCommandPool(device_, cmdPool_, nullptr); cmdPool_ = nullptr; }
    vkDestroyDevice(device_, nullptr); device_ = nullptr;
    physicalDevice_ = nullptr;
  }
  if (instance_) {
    if (surface_) { vkDestroySurfaceKHR(instance_, surface_, nullptr); surface_ = nullptr; }
    if (debug_) { vkDestroyDebugUtilsMessengerEXT(instance_, debug_, nullptr); debug_ = nullptr; }
    vkDestroyInstance(instance_, nullptr); instance_ = nullptr;
  }
}

void hlgl::Context::queueDeletion(DelQueueItem item) {
  delQueues_.back().push_back(item);
}

void hlgl::Context::flushDelQueue() {

  for (auto& varItem : delQueues_.front()) {
    if (std::holds_alternative<DelQueueBuffer>(varItem)) {
      auto item = std::get<DelQueueBuffer>(varItem);
      if (item.allocation && item.buffer) vmaDestroyBuffer(allocator_, item.buffer, item.allocation);
    }
    else if (std::holds_alternative<DelQueueTexture>(varItem)) {
      auto item = std::get<DelQueueTexture>(varItem);
      if (item.sampler) vkDestroySampler(device_, item.sampler, nullptr);
      if (item.view) vkDestroyImageView(device_, item.view, nullptr);
      if (item.allocation && item.image) vmaDestroyImage(allocator_, item.image, item.allocation);
    }
    else if (std::holds_alternative<DelQueuePipeline>(varItem)) {
      auto item = std::get<DelQueuePipeline>(varItem);
      if (item.pipeline) vkDestroyPipeline(device_, item.pipeline, nullptr);
      if (item.layout) vkDestroyPipelineLayout(device_, item.layout, nullptr);
      if (item.descLayout) vkDestroyDescriptorSetLayout(device_, item.descLayout, nullptr);
    }
  }
  for (size_t i {0}; (i+1) < delQueues_.size(); ++i) {
    delQueues_[i] = std::move(delQueues_[i+1]);
  }
  delQueues_.back() = {};
}

void hlgl::Context::flushAllDelQueues() {
  for (size_t i {0}; i < delQueues_.size(); ++i) {
    flushDelQueue();
  }
}