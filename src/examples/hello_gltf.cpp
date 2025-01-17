#include <hlgl/hlgl-plus.h>
#include <GLFW/glfw3.h>
#include <fmt/base.h>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

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

layout (buffer_reference, std430) readonly buffer VertexBuffer {
  Vertex vertices[];
};

layout (push_constant) uniform Constants {
  mat4 matrix;
  VertexBuffer vertexBuffer;
} pushConstants;

void main() {
  Vertex vert = pushConstants.vertexBuffer.vertices[gl_VertexIndex];

  gl_Position = pushConstants.matrix * vec4(vert.position, 1);
  outColor = vert.color.rgb;
  outTexCoord = vec2(vert.u, vert.v);
}
)VertexShader";

const char* object_frag = R"FragmentShader(
#version 450

layout (location = 0) in vec3 inColor;
layout (location = 0) out vec4 outColor;

void main() {
  outColor = vec4(inColor, 1);
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

  // Create the pipeline for the graphics shaders.
  hlgl::GraphicsPipeline graphicsPipeline(context, hlgl::GraphicsPipelineParams{
    .vertexShader   = {.sName = "object.vert", .sGlsl = object_vert },
    .fragmentShader = {.sName = "object.frag", .sGlsl = object_frag },
    .eCullMode = hlgl::CullMode::None,
    .colorAttachments = {hlgl::ColorAttachment{.format = context.getDisplayFormat()}}, });

  if (!graphicsPipeline) {
    fmt::println("HLGL graphics pipeline creation failed.");
    return 1;
  }

  struct DrawPushConsts {
    glm::mat4 matrix{};
    hlgl::DeviceAddress vertexBuffer{0};
  } drawPushConsts;

  // Load the mesh.
  auto meshes = hlgl::Mesh::loadGltf(context, "../../assets/meshes/basicmesh.glb");
  if (meshes.size() == 0) {
    fmt::println("HLGL failed to load assets.");
    return 1;
  }

  // Camera view matrix.
  glm::mat4 view = glm::translate(glm::vec3{0, 0, -5});
  
  // Loop until the window is closed.
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Begin the frame.  When the Frame object is destroyed at the end of this scope, the frame will be presented to the screen.
    if (hlgl::Frame frame = context.beginFrame(); frame) {

      // Calculate the perspective matrix based on the current aspect ratio.
      glm::mat4 proj = glm::perspective(glm::radians(70.f), context.getDisplayAspectRatio(), 10000.f, 0.01f);
      // Invert the Y direction on the projection matrix so we're more similar to opengl and gltf axis.
      //proj [1][1] *= -1;
      drawPushConsts.matrix = proj * view;

      // Draw the model.
      frame.beginDrawing({hlgl::AttachColor{
        .texture = frame.getSwapchainTexture(),
        .clear = hlgl::ColorRGBAf{0.3f, 0.1f, 0.2f, 1.f} }});
      frame.bindPipeline(graphicsPipeline);
      for (auto& mesh : meshes) {
        drawPushConsts.vertexBuffer = mesh.getVboDeviceAddress();
        frame.pushConstants(&drawPushConsts, sizeof(DrawPushConsts));
        mesh.draw(frame);
      }
    }
  }

  meshes.clear();

  return 0;
}