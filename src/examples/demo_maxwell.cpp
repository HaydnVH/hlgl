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
  GLFWwindow* window = glfwCreateWindow(800, 600, "Hello Maxwell HLGL", nullptr, nullptr);
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
  auto model = assetCache.loadModel("../../assets/models/maxwell.glb");

  hlgl::DrawContext draws;
  model->draw(glm::identity<glm::mat4>(), draws);

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
      glm::mat4 view = glm::translate(glm::vec3{0, -8, -40}) * glm::rotate((float)(runningTime * glm::pi<double>() * 2.0) * -0.26f, glm::vec3{0,1,0});
      // Calculate the perspective matrix based on the current aspect ratio.
      glm::mat4 proj = glm::perspective(glm::radians(40.f), context.getDisplayAspectRatio(), 0.01f, 10000.f);
      // Invert the Y direction on the projection matrix so we're more similar to opengl and gltf axis.
      proj [1][1] *= -1;
      drawPushConsts.matrix = proj * view;

      // Draw the model.
      frame.beginDrawing({hlgl::AttachColor{
        .texture = frame.getSwapchainTexture(),
        .clear = hlgl::ColorRGBAf{1.f, 1.f, 1.f, 1.f} }},
        hlgl::AttachDepthStencil{.texture = &depthAttachment, .clear = hlgl::DepthStencilClearVal{.depth = 1.0f, .stencil = 0}});

      for (auto& draw : draws.opaqueDraws) {
        frame.bindPipeline(draw.material->pipeline.get());
        frame.pushBindings({hlgl::ReadBuffer{draw.vertexBuffer, 0}, hlgl::ReadTexture{draw.material->textures.baseColor.get(), 1}}, false);
        drawPushConsts.matrix *= draw.transform;
        frame.pushConstants(&drawPushConsts, sizeof(DrawPushConsts));
        frame.drawIndexed(draw.indexBuffer, draw.indexCount, 1, draw.firstIndex, 0, 1);
      }

      for (auto& draw : draws.nonOpaqueDraws) {
        frame.bindPipeline(draw.material->pipeline.get());
        frame.pushBindings({hlgl::ReadBuffer{draw.vertexBuffer, 0}, hlgl::ReadTexture{draw.material->textures.baseColor.get(), 1}}, false);
        drawPushConsts.matrix = proj * view * draw.transform;
        frame.pushConstants(&drawPushConsts, sizeof(DrawPushConsts));
        frame.drawIndexed(draw.indexBuffer, draw.indexCount, 1, draw.firstIndex, 0, 1);
      }
    }
  }

  return 0;
}