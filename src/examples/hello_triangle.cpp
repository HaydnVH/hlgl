#include <hlgl.h>
#include <GLFW/glfw3.h>

#include <iostream>

const char* hello_triangle_slang = R"Shader(
struct VSInput {};

struct VSOutput {
  float4 pos : SV_POSITION;
  float3 col;
  float2 uv;
};

[vk::binding(0,1)]
Sampler2D textures[];

static float2 positions[3] = {{0.5, 0.5}, {0.0, -0.5}, {-0.5, 0.5}};
static float3 colors[3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};

[shader("vertex")]
VSOutput main(uint vertIndex : SV_VertexID) {
  VSOutput output;
  output.pos = float4(positions[vertIndex], 0.0, 1.0);
  output.col = colors[vertIndex];
  output.uv = positions[vertIndex];
  return output;
}

[shader("fragment")]
float4 main(VSOutput input, uniform uint descIndex) {
  float4 color = textures[NonUniformResourceIndex(descIndex)].Sample(input.uv);
  return float4(input.col, 1.0);
}
)Shader";

int main(int, char**) {

  // Create the window.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(1920, 1080, "Hello Triangle HLGL", nullptr, nullptr);
  if (!window) {
    std::cerr << "Window creation failed.\n";
    return 1;
  }

  // Create the HLGL context.
  if (!hlgl::initContext(hlgl::InitContextParams{
    .window = window,
    .debugCallback = [](hlgl::DebugSeverity severity, std::string_view message) {
      switch (severity) {
        case hlgl::DebugSeverity::Fatal: std::cout << "\x1b[30m\x1b[38;2;255;0;0m"; break;
        case hlgl::DebugSeverity::Error: std::cout << "\x1b[30m\x1b[38;2;200;120;120m"; break;
        case hlgl::DebugSeverity::Warning: std::cout << "\x1b[30m\x1b[38;2;200;200;120m"; break;
        case hlgl::DebugSeverity::Info: std::cout << "\x1b[30m\x1b[38;2;120;200;120m"; break;
        case hlgl::DebugSeverity::Verbose: std::cout << "\x1b[30m\x1b[38;2;120;200;200m"; break;
        case hlgl::DebugSeverity::ObjectCreation: std::cout << "\x1b[30m\x1b[38;2;120;120;200m"; break;
      }
      std::cout << "[HLGL] " << message << "\x1b[0m" << std::endl;
    },
    .requiredFeatures = hlgl::Feature::Validation }))
  {
    std::cerr << "HLGL context creation failed.\n";
    return 1;
  }
  else
  {
    hlgl::Shader shader(hlgl::Shader::CreateParams{.src = hello_triangle_slang, .debugName = "hello_triangle.slang"});
    if (!shader) {
      std::cerr << "HLGL shader creation failed.\n";
    }

    hlgl::Pipeline pipeline(hlgl::Pipeline::GraphicsParams{
      .vertShader = {.shader = &shader},
      .fragShader = {.shader = &shader},
      .colorAttachments = {hlgl::ColorAttachmentInfo{.format = hlgl::getDisplayFormat()}}
    });
    if (!pipeline) {
      std::cerr << "HLGL graphics pipeline creation failed.\n";
      return 1;
    }

    // Loop until the window is closed.
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      // Begin the frame.
      hlgl::Result result = hlgl::beginFrame();
      if (result == hlgl::Result::Shutdown)
        break;
      else if (result == hlgl::Result::Success)
      {
        hlgl::beginDrawing({hlgl::ColorAttachment{
          .texture = hlgl::getFrameSwapchainImage(),
          .clear = hlgl::ColorRGBAf{0.5f, 0.0f, 0.5f, 1.0f}}});
        
        hlgl::bindPipeline(&pipeline);

        uint32_t descIndex {0};
        hlgl::pushConstants(&descIndex, sizeof(descIndex));
        
        hlgl::draw(3);
        hlgl::endFrame();
      }
    }
  }
  hlgl::shutdownContext();

  return 0;
}