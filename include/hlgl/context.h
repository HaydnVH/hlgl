#pragma once

#include <cstdint>
#include <functional>
#include "buffer.h"
#include "frame.h"
#include "pipeline.h"
#include "texture.h"
#include "types.h"

namespace hlgl {

// GpuProperties encapsulates the properties of the physical GPU that the library is using.
struct GpuProperties {
  // The name of the GPU.
  std::string name {""};

  // Which API version is HLGL running.
  uint32_t apiVerMajor {0}, apiVerMinor {0}, apiVerPatch {0};

  // The version of the graphics driver for this GPU.
  uint32_t driverVerMajor {0}, driverVerMinor {0}, driverVerPatch {0};

  // How much device-local VRAM the GPU has access to.
  uint64_t deviceMemory {0};

  // The type (CPU, Virtual, Itegrated, or Discrete) of the GPU.
  GpuType type;

  // The vendor that built the GPU.
  GpuVendor vendor {};

  // Which features the GPU supports.
  Features supportedFeatures {};

  // Which features the GPU supports and are currently enabled.
  Features enabledFeatures {};
};

namespace context {

  // InitParams encapsulates parameters used in the creation of a Context.
  struct InitParams {
    // Required!
    // The handle to the window which the renderer should draw to.
    WindowHandle window;

    // The name of the application.  Optional.
    const char* appName {nullptr};

    // The version of the application.  Optional, defaults to {0,0,0}.
    uint32_t appVerMajor {0}, appVerMinor {0}, appVerPatch {0};

    // The name of the engine that the application is running on.  Optional.
    const char* engineName {nullptr};

    // The version of the engine that the application is running on.  Optional, defaults to {0,0,0}.
    uint32_t engineVerMajor {0}, engineVerMinor {0}, engineVerPatch {0};

    // Callback function pointer so HLGL can print messages to some output.
    // Optional, when not provided HLGL will not print anything and validation cannot be enabled.
    DebugCallback debugCallback{};

    // The name of a GPU to prefer over all others installed in the system, regardless of capabilities.
    // Optional, when not provided (or not found) the most appropriate GPU is used (requested features > device type > VRAM).
    const char* preferredGpu {nullptr};

    // The set of optional features which should be enabled if supported by the GPU.
    Features preferredFeatures {};

    // The set of features which must be enabled, causing initialization to fail in their absence.
    Features requiredFeatures {};

    // The Vsync mode which should be used initally.  This can be changed after context initialization.
    VsyncMode vsync {VsyncMode::Fifo};

    // Whether HDR should be enabled initially.  This can be changed after context intitialization.
    bool hdr {false};
  };

  // Initialize the HLGL context.
  // Returns false if initialization fails for any reason.  If this happens, attempted further usage of HLGL will likely crash your program.
  bool init(InitParams params);

  // Shuts down HLGL, cleaning up any remaining objects and GPU resources.
  void shutdown();

  // Returns the GpuProperties which describes the currently-used GPU.
  const GpuProperties& getGpuProperties();
  
  // Returns the display's aspect ratio (width / height).
  float getDisplayAspectRatio();
  
  // Gets the display's surface format.
  Format getDisplayFormat();
  
  // Gets the current size of the display, storing width and height in 'w' and 'h' respectively.
  void getDisplaySize(uint32_t& w, uint32_t& h);

  // Call this after the window is resized to notify HLGL that it will need to resize the swapchain and any screen-sized framebuffer textures.
  // Without this, HLGL's update loop MIGHT correctly detect that the window surface has resized and act appropriately, but it also might not.
  void setDisplaySize(uint32_t newWidth, uint32_t newHeight);

  // Requests that the given vsync mode be enabled.  Might default to FIFO if the requested mode isn't supported.
  void setVsync(VsyncMode mode);

  // Sets whether HDR should be requested or not.  Might default to false if the requested mode isn't supported.
  void setHdr(bool val);

  // Returns whether the given format is valid for a depth-stencil buffer on the current physical device.
  bool isDepthFormatSupported(Format format);

  // Starts a new Imgui frame.
  void imguiNewFrame();

} // namespace context

} // namespace hlgl