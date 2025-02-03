#include <hlgl/hlgl-plus.h>
#include <GLFW/glfw3.h>
#include <fmt/base.h>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <chrono>

int main(int, char**) {
  // Create the window.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(800, 600, "Hello Scene HLGL", nullptr, nullptr);
  if (!window) {
    fmt::println("Window creation failed.");
    return 1;
  }
  glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

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
  auto model = assetCache.loadModel("../../assets/models/structure.glb");

  hlgl::DrawContext draws;
  model->draw(glm::identity<glm::mat4>(), draws);

  struct DrawPushConsts {
    glm::mat4 matrix{};
  } drawPushConsts;

  glm::vec3 cameraPos {0,0,0};
  float cameraPitch {0.0f}, cameraYaw {0.0f};

  glm::mat4 cameraView { glm::identity<glm::mat4>() };
  glm::mat4 cameraProj { glm::perspective(glm::radians(90.0f), context.getDisplayAspectRatio(), 0.01f, 10000.0f) };
  cameraProj [1][1] *= -1; // Invert the Y direction on the projection matrix so we're more similar to opengl and gltf axis.
  bool mouseHeld {false};
  double mouseX {0.0}, mouseY {0.0};


  auto then = std::chrono::high_resolution_clock::now();
  double runningTime {0.0};

  // Loop until the window is closed.
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    auto now = std::chrono::high_resolution_clock::now();
    double deltaTime = std::chrono::duration<double>(now - then).count();
    runningTime += deltaTime;
    then = now;

    // Read mouse movement and translate it into camera pitch/yaw.
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
      if (!mouseHeld) {
        mouseHeld = true;
        glfwGetCursorPos(window, &mouseX, &mouseY);
      }
      else {
        double newMouseX {0.0}, newMouseY {0.0};
        glfwGetCursorPos(window, &newMouseX, &newMouseY);
        cameraPitch += (float)(newMouseY - mouseY);
        cameraYaw += (float)(newMouseX - mouseX);
        mouseX = newMouseX;
        mouseY = newMouseY;
      }
    }
    else
      mouseHeld = false;

    cameraView =
      glm::rotate(glm::radians(cameraPitch), glm::vec3 {1,0,0}) *
      glm::rotate(glm::radians(cameraYaw), glm::vec3{0,1,0});

    // Read keyboard state to move the camera around.
    glm::vec3 movement {0,0,0};
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      movement += glm::vec3{0, 0,-1};
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      movement += glm::vec3{0, 0, 1};
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      movement += glm::vec3{-1, 0, 0};
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      movement += glm::vec3{ 1, 0, 0};
    movement = glm::rotate(glm::radians(-cameraYaw), glm::vec3{0,1,0}) * glm::vec4(movement, 0);
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
      movement += glm::vec3{0, 1, 0};
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
      movement += glm::vec3{0,-1, 0};

    cameraPos += movement * (float)deltaTime * 20.0f;
    cameraView = cameraView * glm::translate(-cameraPos);

    // Begin the frame.  When the Frame object is destroyed at the end of this scope, the frame will be presented to the screen.
    if (hlgl::Frame frame = context.beginFrame(); frame) {

      // Recalculate the projection matrix in case the aspect ratio changed.
      cameraProj = glm::perspective(glm::radians(40.f), context.getDisplayAspectRatio(), 0.01f, 10000.f);
      cameraProj [1][1] *= -1;
      
      drawPushConsts.matrix = cameraProj * cameraView;

      // Draw the model.
      frame.beginDrawing({hlgl::AttachColor{
        .texture = frame.getSwapchainTexture(),
        .clear = hlgl::ColorRGBAf{0.3f, 0.0f, 0.3f, 1.f} }},
        hlgl::AttachDepthStencil{.texture = &depthAttachment, .clear = hlgl::DepthStencilClearVal{.depth = 1.0f, .stencil = 0}});

      for (auto& draw : draws.opaqueDraws) {
        frame.bindPipeline(draw.material->pipeline.get());
        frame.pushBindings({hlgl::ReadBuffer{draw.vertexBuffer, 0}, hlgl::ReadTexture{draw.material->textures.baseColor.get(), 1}}, false);
        drawPushConsts.matrix = cameraProj * cameraView * draw.transform;
        frame.pushConstants(&drawPushConsts, sizeof(DrawPushConsts));
        frame.drawIndexed(draw.indexBuffer, draw.indexCount, 1, draw.firstIndex, 0, 1);
      }

      for (auto& draw : draws.nonOpaqueDraws) {
        frame.bindPipeline(draw.material->pipeline.get());
        frame.pushBindings({hlgl::ReadBuffer{draw.vertexBuffer, 0}, hlgl::ReadTexture{draw.material->textures.baseColor.get(), 1}}, false);
        drawPushConsts.matrix = cameraProj * cameraView * draw.transform;
        frame.pushConstants(&drawPushConsts, sizeof(DrawPushConsts));
        frame.drawIndexed(draw.indexBuffer, draw.indexCount, 1, draw.firstIndex, 0, 1);
      }
    }
  }

  return 0;
}