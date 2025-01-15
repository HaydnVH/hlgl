#pragma once

#include <cstdint>
#include <functional>
#include "wcgl-buffer.h"
#include "wcgl-frame.h"
#include "wcgl-pipeline.h"
#include "wcgl-texture.h"
#include "wcgl-types.h"

namespace wcgl {

// GpuProperties encapsulates the properties of the physical GPU that the library is using.
struct GpuProperties {
  // The name of the GPU.
  std::string sName {""};

  // The type (CPU, Virtual, Itegrated, or Discrete) of the GPU.
  GpuType eType;

  // How much device-local VRAM the GPU has access to.
  size_t iDeviceMemory {0};

  // Which API version is HLGL running.
  Version apiVersion {};

  // The vendor that built the GPU.
  Vendor eVendor {};

  // The version of the graphics driver for this GPU.
  Version driverVersion {};

  // Which features the GPU supports.
  Features supportedFeatures {};

  // Which features the GPU supports and are currently enabled.
  Features enabledFeatures {};
};

// ContextParams encapsulates parameters used in the creation of a Context.
struct ContextParams {
  // Required!
  // The handle to the window which the renderer should draw to.
  WindowHandle pWindow;

  // The name of the application.  Optional, defaults to "".
  const char* sAppName {""};

  // The version of the application.  Optional, defaults to {0,0,0}.
  Version appVersion{0,0,0};

  // The name of the engine that the application is running on.  Optional, defaults to "".
  const char* sEngineName {""};

  // The version of the engine that the application is running on.  Optional, defaults to {0,0,0}.
  Version engineVersion{0,0,0};

  // Callback function pointer so WCGL can print messages to some output.
  // Optional, when not provided WCGL will not print anything and validation cannot be enabled.
  DebugCallback fnDebugCallback{};

  // The name of a GPU to prefer over all others installed in the system, regardless of capabilities.
  // Optional, when not provided (or not found) the most appropriate GPU is used (requested features > device type > VRAM).
  const char* sPreferredGpu {nullptr};

  // The set of optional features which should be enabled if supported by the GPU.
  Features preferredFeatures {};

  // The set of features which must be enabled, causing initialization to fail in their absence.
  Features requiredFeatures {};
};

// The Context represents backend functionality managed by the library.
// It tracks and manages global state and enables the creation of other objects.
class Context {
  friend class Buffer;
  friend class Texture;
  friend class Pipeline;
  friend class ComputePipeline;
  friend class GraphicsPipeline;
  friend class Frame;

  Context(const Context&) = delete;
  Context& operator= (const Context&) = delete;

public:
  Context(Context&&) = default;
  Context& operator=(Context&&) = default;

  Context(ContextParams params);
  ~Context();

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

  const GpuProperties gpuProperties() const { return gpu_; }

  void displayResized(uint32_t newWidth, uint32_t newHeight) { displayWidth_ = newWidth; displayHeight_ = newHeight; }
  std::pair<uint32_t, uint32_t> getDisplaySize() const { return {displayWidth_, displayHeight_}; }
  Format getDisplayFormat();

  void imguiNewFrame();
  Frame beginFrame();

private:
  bool initSuccess_ {false};
  bool inFrame_ {false};
  GpuProperties gpu_ {};
  uint32_t displayWidth_ {0}, displayHeight_ {0};
  bool displayHdr_ {false}, displayVsync_ {false};

#if defined WCGL_GRAPHICS_API_VULKAN
  VkInstance instance_ {nullptr};
  bool initInstance(const VkApplicationInfo& appInfo, Features preferredFeatures, Features requiredFeatures);
  VkDebugUtilsMessengerEXT debug_ {nullptr};
  bool initDebug();
  VkSurfaceKHR surface_ {nullptr};
  bool initSurface(WindowHandle window);
  VkPhysicalDevice physicalDevice_ {nullptr};
  bool pickPhysicalDevice(const char* preferredPhysicalDevice, Features preferredFeatures, Features requiredFeatures);
  VkDevice device_ {nullptr};
  bool initDevice(Features preferredFeatures, Features requiredFeatures);
  uint32_t graphicsQueueFamily_ {UINT32_MAX};
  uint32_t presentQueueFamily_ {UINT32_MAX};
  uint32_t computeQueueFamily_ {UINT32_MAX};
  uint32_t transferQueueFamily_ {UINT32_MAX};
  VkQueue graphicsQueue_ {nullptr};
  VkQueue presentQueue_ {nullptr};
  VkQueue computeQueue_ {nullptr};
  VkQueue transferQueue_ {nullptr};
  bool initQueues();
  VkCommandPool cmdPool_ {nullptr};
  bool initCmdPool();
  VkDescriptorPool descPool_ {nullptr};
  bool initDescPool();
  VkSwapchainKHR swapchain_ {nullptr};
  VkExtent2D swapchainExtent_ {};
  VkFormat swapchainFormat_ {};
  uint32_t swapchainIndex_ {0};
  std::vector<Texture> swapchainTextures_;
  bool resizeSwapchain();
  struct FrameInFlight {
    VkCommandBuffer cmd {nullptr};
    VkSemaphore imageAvailable {nullptr};
    VkSemaphore renderFinished {nullptr};
    VkFence fence {nullptr}; };
  std::array<FrameInFlight, 2> frames_;
  uint32_t frameIndex_ {0};
  bool initFrames();
  VkCommandBuffer getCommandBuffer() { return frames_[frameIndex_].cmd; }
  VmaAllocator allocator_ {nullptr};
  bool initAllocator();
  bool initImGui(WindowHandle window);

  void destroyBackend();


  // Checks if the swapchain needs to be resized, and resizes it if so.
  // Returns true if the swapchain was resized, false otherwise.
  bool resizeIfNeeded(uint32_t width, uint32_t height, bool hdr, bool vsync);

  // Submits a command to be executed outside of a frame context.
  void immediateSubmit(const std::function<void(VkCommandBuffer)>& func) const;

#endif // defined WCGL_GRAPHICS_API_*
};

}