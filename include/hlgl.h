#ifndef HLGL_H
#define HLGL_H

#include "hlgl/hlgl-base.h"
#include "hlgl/hlgl-buffer.h"
#include "hlgl/hlgl-pipeline.h"
#include "hlgl/hlgl-shader.h"
#include "hlgl/hlgl-texture.h"

namespace hlgl {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Functions

struct                InitContextParams {
  WindowHandle window;                                                  // Handle/pointer to the window which the renderer should draw to.  Required!
  const char* appName {nullptr};                                        // Name of the application.  Optional.
  struct {uint32_t major {0}, minor {0}, patch {0};} appVer {};         // Version of the application.  Defaults to {0,0,0}.
  const char* engineName {nullptr};                                     // Name of the engine that the application is running on.  Optional.
  struct {uint32_t major {0}, minor {0}, patch {0};} engineVer {};      // Version of the engine that the application is running on.  Defaults to {0,0,0}.
  DebugCallbackFunc debugCallback {nullptr};                            // Callback function so HLGL can print messages to some output.  Optional, if not provided then HLGL wont print anything.
  const char* preferredGpu {nullptr};                                   // Name of the GPU which should be preferred regardless of capability.  Optional, when not provided (or not found) the best GPU will be chosen.
  Features preferredFeatures {Feature::None};                           // The set of optional features which should be enabled if supported by the GPU.
  Features requiredFeatures {Feature::None};                            // The set of features which must be enabled, causing initialization to fail in their absence.
  VsyncMode vsync {VsyncMode::Fifo};                                    // The Vsync mode which should be used initally.  This can be changed after context initialization.
  bool hdr {false};                                                     // Whether HDR should be enabled initially.  This can be changed after context intitialization.
  };
bool                  initContext(InitContextParams params);                                    // Initialize the HLGL context.  Returns false if initialization fails, in which case the application should close.
void                  shutdownContext();                                                        // Shuts down the HLGL context, cleaning up any remaining objects and GPU resources.

// Starts a new ImGui frame.
// This calls the appropriate backend's "*_NewFrame()" functions, as well as "ImGui::NewFrame()".
// After "hlgl::imguiNewFrame()", call your gui functions, followed by "ImGui::Render()".
// This should be done BEFORE the HLGL frame, which only draws the rendered ImGui state on top of the screen.
void                  imguiNewFrame();

Result                beginFrame();                                                             // Begins a new frame.  Returns Success if the frame should be drawn, SkipFrame if it should be skipped, and Error if the program should stop.
void                  beginDrawing(                                                             // Binds the given attachments and begins a drawing pass.
                        std::initializer_list<ColorAttachment> colorAttachments,
                        std::optional<DepthAttachment> depthAttachment = std::nullopt);                                       
void                  bindPipeline(Pipeline* pipeline);                                         // Binds the given pipeline, allowing it to be used for dispatch calls.
struct                BlitRegion {
  bool screenRegion {false}; // If true, the blit region will match the display size regardless of the image sizes.
  uint32_t mipLevel {0}, baseLayer {0}, layerCount {1};
  uint32_t x{0}, y{0}, z{0};
  uint32_t w{UINT32_MAX}, h{UINT32_MAX}, d{UINT32_MAX};
  };
void                  blitImage(                                                                // Blits (copies) the contents of one image (src) to another image (dst).
                        Texture* dst, Texture* src,
                        BlitRegion dstRegion, BlitRegion srcRegion,
                        bool filterLinear = false);
void                  dispatch(                                                                 // Executes the currently bound compute pipeline using the given group counts.
                        uint32_t groupCountX,
                        uint32_t groupCountY,
                        uint32_t groupCountZ);
void                  draw(                                                                     // Draws 'vertexCount' vertices according to the currently bound graphics pipeline.
                        uint32_t vertexCount,
                        uint32_t instanceCount = 1,
                        uint32_t firstVertex = 0,
                        uint32_t firstInstance = 0);
void                  drawIndexed(                                                              // Binds an index buffer and uses it to draw vertices.
                        uint32_t indexCount,                          // 'indexCount': The number of vertices to draw.
                        Buffer* indexBuffer,                          // The index buffer to bind.
                        uint8_t indexSize = 4,                        // The size, in bytes, of each index (1 = uint8, 2 = uint16, 4 = uint32)
                        DeviceSize offset = 0,
                        uint32_t instanceCount = 1,
                        uint32_t firstIndex = 0,
                        uint32_t vertexOffset = 0,
                        uint32_t firstInstance = 0);
void                  drawIndirect(                                                             // Executes 'drawCount' draw calls using draw commands contained in 'drawBuffer'.
                        Buffer* drawBuffer,
                        DeviceSize drawOffset,
                        uint32_t drawCount,
                        uint32_t stride);
void                  drawIndexedIndirect(                                                      // Executes 'drawCount' draw calls using indexed draw commands contained in 'drawBuffer'.
                        Buffer* drawBuffer,
                        DeviceSize drawOffset,
                        uint32_t drawCount,
                        uint32_t stride);
void                  drawIndirectCount(                                                        // Uses 'countBuffer' to determine how many objects to draw using draw commands in 'drawBuffer'.
                        Buffer* drawBuffer,
                        DeviceSize drawOffset,
                        Buffer* countBuffer,
                        DeviceSize countOffset,
                        uint32_t maxDraws,
                        uint32_t stride);
void                  drawIndexedIndirectCount(                                                 // Uses 'countBuffer' to determine how many objects to draw using indexed draw commands in 'drawBuffer'.
                        Buffer* drawBuffer,
                        DeviceSize drawOffset,
                        Buffer* countBuffer,
                        DeviceSize countOffset,
                        uint32_t maxDraws,
                        uint32_t stride);
void                  endDrawing();                                                             // Ends the current drawing pass.
void                  endFrame();                                                               // Ends the frame, executing command buffers and displaying the swapchain image to the screen.
int64_t               getFrameCounter();                                                        // Gets the counter for the current frame (increments by one for each drawn frame).
Texture*              getFrameSwapchainImage();                                                 // Gets the current swapchain image which this frame will draw to.
void                  pushConstants(const void* data, size_t size);                             // Pushes the provided data to the currently bound pipeline as a push constant block.

float                 getDisplayAspectRatio();                                                  // Gets the aspect ratio of the current display (width / height).
ImageFormat           getDisplayFormat();                                                       // Gets the image format of the display's surface.
void                  getDisplaySize(uint32_t& w, uint32_t& h);                                 // Gets the of the display.  Width is stored in 'w' and height is stored in 'h'.
const GpuProperties&  getGpuProperties();                                                       // Gets the properties of the GPU being used by HLGL.
VsyncMode             getVsync();                                                               // Gets the current vsync mode.
bool                  isDepthFormatSupported(ImageFormat format);                               // Returns true if the provided format is supported as a depth-stencil format by the GPU being used by HLGL.
bool                  isHdrEnabled();                                                           // Returns true if HDR rendering is currently enabled.
inline bool           isValidationEnabled()                                                     // Returns true if validation is enabled.  Equivalent to (getGpuProperties().enabledFeatures & Feature::Validation).
                        { return (getGpuProperties().enabledFeatures & Feature::Validation); }

void                  setDisplaySize(uint32_t w, uint32_t h);                                   // After the display resizes, use this to provide a hint for what size the new swapchain should be.
void                  setHdr(bool mode);                                                        // Sets whether to request an HDR surface.  If HDR support isn't available, it will be disabled.
void                  setVsync(VsyncMode mode);                                                 // Sets the requested vsync mode.  If the requested mode isn't available, the swapchain may default to "Fifo".

} // namespace hlgl
#endif // HLGL_H
