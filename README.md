# HLGL
A **H**igh **L**evel **G**raphics **L**ibrary written in modern C++ for modern graphics APIs.  It started its life as the Render Hardware Interface layer for my custom game engine "Witchcraft" but was separated out into its own project.  HLGL attempts to simplify graphics programming down to the absolute bare minimum that a programmer needs to care about.  Parameter structs with default values allow the user to use designated initializers to only modify what's neccessary and expose functionality that's only used occasionally.  HLGL does make certain assumptions and take control of certain operations, so you wont have as much control compared to a verbose API like Vulkan, but hopefully you'll find that HLGL does so in a sensible way that doesn't take away the programmer's ability to make meaningful artistic or algorithmic decisions.

Take a look at the sample programs in `src/examples` for examples of how to use HLGL.  The basic workflow generally goes like this:
- First, call `hlgl::initContext(...);` to initialize the instance, device, swapchain, and any other global state.
- Create some buffers as `hlgl::Buffer` objects to store geometry data and uniforms.
- Create some textures as `hlgl::Texture` objects to store texture data and framebuffers, including a depth buffer if desired.
- Create some shaders as `hlgl::Shader` objects to compile Slang source code or Spir-V bytecode.  A single Shader object can contain more than one stage.
- Create some pipelines as `hlgl::Pipeline` objects using your shaders.  These can be compute pipelines or graphics pipelines.
- In your main loop, start a new frame using `hlgl::beginFrame(...)`.  Many functions can only be run while a frame is active.
- Bind pipelines, update buffers, and issue barriers depending on what you plan to do during the frame.
- Excute compute shaders using `hlgl::dispatch(...)` while a compute pipeline is bound.
- Begin a drawing pass using `hlgl::beginDrawing(...)`, binding framebuffers and setting clear values.
- Draw geometry using `hlgl::draw(..)` or a similar function.  Indexed and indirect drawing is supported.
- Finalize and display the frame using `hlgl::endFrame(...)`.

### Installation

Clone this repository and make sure to include the `--recursive` flag to include the third-party dependencies via git submodules.  The only external dependency is the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/).  Use CMake to build the project; Visual Studio or VSCode are recommended.  Windows and Linux are supported and should both work "out of the box".
