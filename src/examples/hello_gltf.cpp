#include <hlgl/hlgl-plus.h>
#include <GLFW/glfw3.h>
#include <fmt/base.h>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtx/transform.hpp>
#include <chrono>

int main(int, char**) {
  // Create the window.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(800, 600, "Hello GLTF HLGL", nullptr, nullptr);
  if (!window) {
    fmt::println("Window creation failed.");
    return 1;
  }

  // Create the HLGL context.
  hlgl::Context context(hlgl::ContextParams{
    .pWindow = window,
    .fnDebugCallback = [](hlgl::DebugSeverity severity, std::string_view msg) {fmt::println("[HLGL] {}", msg); },
    .requiredFeatures = hlgl::Feature::Validation | hlgl::Feature::BufferDeviceAddress});
  if (!context) {
    fmt::println("HLGL context creation failed.");
    return 1;
  }

  // Create a depth texture to use for z-buffering.
  hlgl::Texture depthAttachment(context, hlgl::TextureParams {
    .bMatchDisplaySize = true,
    .eFormat = hlgl::Format::D32f,
    .usage = hlgl::TextureUsage::Framebuffer,
    .sDebugName = "depthAttachment"});
  if (!depthAttachment) {
    fmt::println("HLGL depth buffer creation failed.");
    return 1;
  }

  // Create the asset cache.
  hlgl::AssetCache assetCache(context);
  assetCache.initDefaultAssets();

  // Load assets.
  auto pipeline = assetCache.loadPipeline("hlgl::pipelines/pbr-opaque");
  auto tex = assetCache.loadTexture("hlgl::textures/missing");
  auto model = assetCache.loadModel("../../assets/meshes/basicmesh.glb");
  hlgl::Mesh* mesh {nullptr};
  if (auto it {model->find("Suzanne")}; it != model->end()) {
    mesh = &it->second;
  }

  struct DrawPushConsts {
    glm::mat4 matrix{};
  } drawPushConsts;

  auto then = std::chrono::high_resolution_clock::now();
  double runningTime {0.0};
  
  // Loop until the window is closed.
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    auto now = std::chrono::high_resolution_clock::now();
    double deltaTime = std::chrono::duration<double>(now - then).count();
    runningTime += deltaTime;
    then = now;

    // Begin the frame.  When the Frame object is destroyed at the end of this scope, the frame will be presented to the screen.
    if (hlgl::Frame frame = context.beginFrame(); frame) {

      // Camera view matrix.
      glm::mat4 view = glm::translate(glm::vec3{0, 0, -5}) * glm::rotate((float)runningTime, glm::vec3{0,1,0});
      // Calculate the perspective matrix based on the current aspect ratio.
      glm::mat4 proj = glm::perspective(glm::radians(70.f), context.getDisplayAspectRatio(), 0.01f, 10000.f);
      // Invert the Y direction on the projection matrix so we're more similar to opengl and gltf axis.
      proj [1][1] *= -1;
      drawPushConsts.matrix = proj * view;

      // Draw the model.
      frame.beginDrawing({hlgl::AttachColor{
        .texture = frame.getSwapchainTexture(),
        .clear = hlgl::ColorRGBAf{0.3f, 0.1f, 0.2f, 1.f} }},
        hlgl::AttachDepthStencil{.texture = &depthAttachment, .clear = hlgl::DepthStencilClearVal{.depth = 1.0f, .stencil = 0}});

      if (mesh) {
        frame.bindPipeline(*pipeline.get());
        frame.pushBindings(0, {hlgl::ReadBuffer{mesh->vertexBuffer(), 0}, hlgl::ReadTexture{tex.get(), 1}}, false);
        frame.pushConstants(&drawPushConsts, sizeof(DrawPushConsts));
        for (auto& submesh : mesh->subMeshes()) {
          frame.drawIndexed(mesh->indexBuffer(), submesh.count, 1, submesh.start, 0, 1);
        }
      }
    }
  }

  return 0;
}