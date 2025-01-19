# HLGL
A **H**igh **L**evel **G**raphics **L**ibrary written in modern C++ for modern graphics APIs.  It started its life as the Vulkan abstraction layer for my custom game engine "Witchcraft" but was separated out into its own project.  HLGL attempts to simplify graphics programming down to the absolute bare minimum that a programmer needs to care about.  Parameter structs with default values allow the user to use designated initializers to only modify what's neccessary and expose functionality that's only used occasionally.  HLGL does make certain assumptions and take control of certain operations, so you wont have as much control compared to a verbose API like Vulkan, but hopefully you'll find that HLGL does so in a sensible way that doesn't take away the programmer's ability to make meaningful artistic or algorithmic decisions.

HLGL is organized into two libraries: `hlgl-core` which provides the basic underlying primitives and operations, and `hlgl-plus` which implements higher-level concepts such as models, materials, lights, and provides default shaders.

### HLGL Core

The `hlgl-core` library provides only 5 basic classes which, together, act as an abstraction for the entire underlying graphics API.  These classes should theoretically be enough to implement whatever graphics project you could imagine, from a small indie game or tool to a sophisticated AAA engine.  They are:

- `Context`, which manages global initialization, cleanup, and state.
- `Buffer`, which represents arbitrary data in GPU memory that can contain vertices, indices, shader uniforms, or whatever else you might need.
- `Texture`, which represents an image residing in GPU memory.  This could be a sampleable texture, a framebuffer, or really any image-like data.
- `Pipeline`, which represents one or more shaders plus additional state required for the execution of said shaders.  Pipelines are separated into `ComputePipeline` and `GraphicsPipeline`.
- `Frame`, which is primarily responsible for the startup and shutdown of frames as they're displayed, and also manages pipeline bindings and draw/dispatch calls.

### HLGL Plus

The `hlgl-plus` library is an expanding collection of high-level, easy-to-use functionality implemented using `hlgl-core`.  Work in progress!

### Installation

Clone this repository and make sure to include the `--recursive` flag to include the third-party dependencies via git submodules.  The only external dependency is the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/).  Use CMake to build the project; Visual Studio or VSCode are recommended.  Windows and Linux are supported and should both work "out of the box".
