#include <hlgl.h>
#include <GLFW/glfw3.h>

#include <print>

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
  GLFWwindow* window = glfwCreateWindow(1920, 1080, "Hello Triangle HLGL", nullptr, nullptr);
  if (!window) {
    std::println("Window creation failed.");
    return 1;
  }

  // Create the HLGL context.
  if (!hlgl::context::init(hlgl::context::InitParams{
    .window = window,
    .debugCallback = [](hlgl::DebugSeverity severity, std::string_view message){std::println("[HLGL] {}", message);},
    .requiredFeatures = hlgl::Feature::Validation}))
  {
    std::println("HLGL context creation failed.");
    return 1;
  }

  // Create the shaders and pipeline.
  hlgl::GraphicsPipeline pipeline(hlgl::GraphicsPipelineParams {
    .vertShader = hlgl::ShaderParams{.src = hello_triangle_vert, .debugName = "hello_triangle.vert"},
    .fragShader = hlgl::ShaderParams{.src = hello_triangle_frag, .debugName = "hello_triangle.frag"},
    .colorAttachments = {hlgl::ColorAttachmentParams{.format = hlgl::context::getDisplayFormat()}},
  });
  if (!pipeline) {
    std::println("HLGL graphics pipeline creation failed.");
    return 1;
  }

  // Loop until the window is closed.
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Begin the frame.  When the Frame object is destroyed at the end of this scope, the frame will be presented to the screen.
    if (hlgl::Frame frame; frame)
    {
      frame.beginDrawing({hlgl::ColorAttachment{
        .texture = &frame.getSwapchainTexture(),
        .clear = hlgl::ColorRGBAf{0.5f, 0.0f, 0.5f, 1.0f}
        }});
      frame.bindPipeline(&pipeline);
      frame.draw(3);
    }
  }

  return 0;
}