#include <hlgl/hlgl-core.h>
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
  GLFWwindow* window = glfwCreateWindow(800, 600, "Hello Triangle HLGL", nullptr, nullptr);
  if (!window) {
    fmt::println("Window creation failed.");
    return 1;
  }

  // Create the HLGL context.
  hlgl::Context context(hlgl::ContextParams{
    .pWindow = window,
    .fnDebugCallback = [](hlgl::DebugSeverity severity, std::string_view message){fmt::println("[HLGL] {}", message);},
    .requiredFeatures = hlgl::Feature::Validation });
  if (!context) {
    fmt::println("HLGL context creation failed.");
    return 1;
  }

  // Create the shaders and pipeline.
  hlgl::Shader vertShader(context, hlgl::ShaderParams{.sGlsl = hello_triangle_vert, .sDebugName = "hello_triangle.vert"});
  hlgl::Shader fragShader(context, hlgl::ShaderParams{.sGlsl = hello_triangle_frag, .sDebugName = "hello_triangle.frag"});
  hlgl::GraphicsPipeline pipeline(context, hlgl::GraphicsPipelineParams {
    .shaders = {&vertShader, &fragShader},
    .colorAttachments = {hlgl::ColorAttachment{.format = context.getDisplayFormat()}},
  });
  if (!pipeline) {
    fmt::println("HLGL graphics pipeline creation failed.");
    return 1;
  }

  // Loop until the window is closed.
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Begin the frame.  When the Frame object is destroyed at the end of this scope, the frame will be presented to the screen.
    if (hlgl::Frame frame = context.beginFrame(); frame)
    {
      frame.beginDrawing({hlgl::AttachColor{
        .texture = frame.getSwapchainTexture(),
        .clear = hlgl::ColorRGBAf{0.5f, 0.0f, 0.5f, 1.0f}
        }});
      frame.bindPipeline(pipeline);
      frame.draw(3);
    }
  }

  return 0;
}