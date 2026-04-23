#include <hlgl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <iostream>

int main(int, char**) {
  // Create the window.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(1920, 1080, "Hello ImGui HLGL", nullptr, nullptr);
  if (!window) {
    std::cout << "Window creation failed.\n";
    return 1;
  }

  // Create the HLGL context.
  if (!hlgl::initContext(hlgl::InitContextParams{
    .window = window,
    .debugCallback = [](hlgl::DebugSeverity severity, std::string_view message){ std::cout << "[HLGL] " << message << std::endl; },
    .requiredFeatures = hlgl::Feature::Validation}))
  {
    std::cout << "HLGL context creation failed.";
    return 1;
  }

  // Loop until the window is closed.
  while (!glfwWindowShouldClose(window)) {

    glfwPollEvents();
    hlgl::imguiNewFrame();

    ImGui::ShowDemoWindow();
    ImGui::Render();

    // Begin the frame.
    hlgl::Result result = hlgl::beginFrame();
    if (result == hlgl::Result::Error)
      break;
    else if (result == hlgl::Result::Success)
    {
      // Begin a drawing pass.
      // Although we aren't drawing anything, it's neccessary to clear the screen.
      hlgl::beginDrawing({hlgl::ColorAttachment{
        .texture = hlgl::getFrameSwapchainImage(),
        .clear = hlgl::ColorRGBAf{0.5f, 0.0f, 0.5f, 1.0f}
        }});

      hlgl::endFrame();
    }
  }

  hlgl::shutdownContext();
  return 0;
}