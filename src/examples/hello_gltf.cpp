#include <hlgl/hlgl-plus.h>
#include <GLFW/glfw3.h>
#include <fmt/base.h>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtx/transform.hpp>
#include <chrono>

const char* object_vert = R"VertexShader(
#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outTexCoord;

struct Vertex {
  vec3 position;
  float u;
  vec3 normal;
  float v;
  vec4 color;
};

layout(binding=0) readonly buffer Vertices { Vertex vertices[]; };

layout (push_constant) uniform Constants {
  mat4 matrix;
} pushConstants;

void main() {
  Vertex vert = vertices[gl_VertexIndex];

  gl_Position = pushConstants.matrix * vec4(vert.position, 1);
  outColor = vert.color.rgb;
  outTexCoord = vec2(vert.u, vert.v);
}
)VertexShader";

const char* object_frag = R"FragmentShader(
#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inTexCoord;
layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D myTexture;

void main() {
  outColor = texture(myTexture, inTexCoord);
//outColor = vec4(inColor, 1);
}
)FragmentShader";

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

  // Create the pipeline for the graphics shaders.
  hlgl::GraphicsPipeline graphicsPipeline(context, hlgl::GraphicsPipelineParams{
    .vertexShader   = {.sName = "object.vert", .sGlsl = object_vert },
    .fragmentShader = {.sName = "object.frag", .sGlsl = object_frag },
    .depthAttachment = hlgl::DepthAttachment{.format = hlgl::Format::D32f, .eCompare = hlgl::CompareOp::LessOrEqual},
    .colorAttachments = {hlgl::ColorAttachment{.format = context.getDisplayFormat()}} });
  if (!graphicsPipeline) {
    fmt::println("HLGL graphics pipeline creation failed.");
    return 1;
  }

  struct DrawPushConsts {
    glm::mat4 matrix{};
  } drawPushConsts;

  // Load the mesh.
  auto meshes = hlgl::Mesh::loadGltf(context, "../../assets/meshes/basicmesh.glb");
  if (meshes.size() == 0) {
    fmt::println("HLGL failed to load gltf asset.");
    return 1;
  }

  // Create a texture.
  hlgl::ColorRGBAb cyan {0, 255, 255, 255};
  hlgl::ColorRGBAb magenta {255, 0, 255, 255};
  std::array<hlgl::ColorRGBAb, 16*16> checkerPixels;
  for (int x {0}; x < 16; ++x) {
    for (int y {0}; y < 16; ++y) {
      checkerPixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? cyan : magenta;
    }
  }
  hlgl::Texture checkerTex(context, hlgl::TextureParams {.iWidth = 16, .iHeight = 16, .iDepth = 1, .eFormat = hlgl::Format::RGBA8i, .usage = hlgl::TextureUsage::Sampler, .pData = &checkerPixels});

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
      frame.bindPipeline(graphicsPipeline);
      frame.pushBindings(0, {hlgl::ReadTexture{&checkerTex, 1}}, true);
      frame.pushConstants(&drawPushConsts, sizeof(DrawPushConsts));
      meshes[2].draw(frame);
    }
  }

  meshes.clear();

  return 0;
}