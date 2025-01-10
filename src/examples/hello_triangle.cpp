#include <wcgl/wcgl.h>
#include <GLFW/glfw3.h>
#include <fmt/format.h>

const char* hello_triangle_vert = R"VertexShader(
#version 450

out gl_PerVertex {
  vec4 gl_Position;
};

layout(location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](
  vec2(0.5, 0.5),
  vec2(0.0, -0.5),
  vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
  vec3(1.0, 0.0, 0.0),
  vec3(0.0, 1.0, 0.0),
  vec3(0.0, 0.0, 1.0)
);

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
  fragColor = colors[gl_VertexIndex];
}
)VertexShader";

const char* hello_triangle_frag = R"FragmentShader(
#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
  outColor = vec4(fragColor, 1.0);
}
)FragmentShader";

int main(int, char**) {

  // Create the window.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(800, 600, "Hello Triangle WCGL", nullptr, nullptr);
  if (!window) {
    fmt::println("Window creation failed.");
    return 1;
  }

  // Create the WCGL context.
  wcgl::Context context(wcgl::ContextParams{
    .pWindow = window,
    .fnDebugCallback = [](wcgl::DebugSeverity severity, std::string_view message){fmt::println("[WCGL] {}", message);},
    .requiredFeatures = wcgl::Feature::Validation });
  if (!context) {
    fmt::println("WCGL context creation failed.");
    return 1;
  }

  // Create the pipeline for the shaders.
  wcgl::Pipeline pipeline(context, wcgl::PipelineParams {
    .colorAttachments = {wcgl::ColorAttachment{.format = context.getDisplayFormat()}},
    .shaders = {
      wcgl::ShaderParams{.sName = "hello_triangle.vert", .sGlsl = hello_triangle_vert},
      wcgl::ShaderParams{.sName = "hello_triangle.frag", .sGlsl = hello_triangle_frag} }
    });
  if (!pipeline) {
    fmt::println("WCGL graphics pipeline creation failed.");
    return 1;
  }

  // Loop until the window is closed.
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Begin the frame.  When the Frame object is destroyed at the end of this scope, the frame will be presented to the screen.
    if (wcgl::Frame frame = context.beginFrame(); frame)
    {
      frame.beginDrawing({wcgl::AttachColor{
        .texture = frame.getSwapchainTexture(),
        .clear = wcgl::ColorRGBAf{0.5f, 0.0f, 0.5f, 1.0f}
        }});
      frame.bindPipeline(pipeline);
      frame.draw(3);
    }
  }

  return 0;
}