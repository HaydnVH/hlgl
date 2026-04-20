#include <hlgl.h>
#include <GLFW/glfw3.h>

#include <print>

const char* hello_triangle_slang = R"Shader(
struct VSInput {};

struct VSOutput {
  float4 pos : SV_POSITION;
  float3 col;
};

static float2 positions[3] = {{0.5, 0.5}, {0.0, -0.5}, {-0.5, 0.5}};
static float3 colors[3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};

[shader("vertex")]
VSOutput main(uint vertIndex : SV_VertexID) {
  VSOutput output;
  output.pos = float4(positions[vertIndex], 0.0, 1.0);
  output.col = colors[vertIndex];
  return output;
}

[shader("fragment")]
float4 main(VSOutput input) {
  return float4(input.col, 1.0);
}
)Shader";

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
  if (!hlgl::initContext(hlgl::InitContextParams{
    .window = window,
    .debugCallback = [](hlgl::DebugSeverity severity, std::string_view message) {
      switch (severity) {
        case hlgl::DebugSeverity::Fatal: std::print("\x1b[30m\x1b[38;2;255;0;0m"); break;
        case hlgl::DebugSeverity::Error: std::print("\x1b[30m\x1b[38;2;200;120;120m"); break;
        case hlgl::DebugSeverity::Warning: std::print("\x1b[30m\x1b[38;2;200;200;120m"); break;
        case hlgl::DebugSeverity::Info: std::print("\x1b[30m\x1b[38;2;120;200;120m"); break;
        case hlgl::DebugSeverity::Verbose: std::print("\x1b[30m\x1b[38;2;120;200;200m"); break;
      }
      std::println("[HLGL] {}", message);
      std::print("\x1b[0m");
    },
    .requiredFeatures = hlgl::Feature::Validation }))
  {
    std::println("HLGL context creation failed.");
    return 1;
  }
  else
  {
    hlgl::Shader shader(hlgl::Shader::CreateParams{.src = hello_triangle_slang, .debugName = "hello_triangle.slang"});
    if (!shader) {
      std::println("HLGL shader creation failed.");
    }

    hlgl::Pipeline pipeline(hlgl::Pipeline::GraphicsParams{
      .vertShader = {.shader = &shader},
      .fragShader = {.shader = &shader},
      .colorAttachments = {hlgl::ColorAttachmentInfo{.format = hlgl::getDisplayFormat()}}
    });
    if (!pipeline) {
      std::println("HLGL graphics pipeline creation failed.");
      return 1;
    }

    // Loop until the window is closed.
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      // Begin the frame.  When the Frame object is destroyed at the end of this scope, the frame will be presented to the screen.
      if (hlgl::Frame* frame {hlgl::beginFrame()}; frame)
      {
        hlgl::beginDrawing(frame, {hlgl::ColorAttachment{
          .texture = hlgl::getFrameSwapchainImage(frame),
          .clear = hlgl::ColorRGBAf{0.5f, 0.0f, 0.5f, 1.0f}}});
        
        hlgl::bindPipeline(frame, &pipeline);
        hlgl::draw(frame, 3);
        hlgl::endFrame(frame);
      }
    }
  }
  hlgl::shutdownContext();

  return 0;
}