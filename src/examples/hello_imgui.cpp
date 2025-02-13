#include <hlgl/hlgl-core.h>
#include <GLFW/glfw3.h>
#include <fmt/format.h>
#include <imgui.h>


int main(int, char**) {
  // Create the window.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(800, 600, "Hello ImGui HLGL", nullptr, nullptr);
  if (!window) {
    fmt::println("Window creation failed.");
    return 1;
  }

  // Create the HLGL context.
  hlgl::Context context(hlgl::ContextParams{
    .pWindow = window,
    .fnDebugCallback = [](hlgl::DebugSeverity severity, std::string_view msg){fmt::println("[HLGL] {}", msg);},
    .requiredFeatures = hlgl::Feature::Validation | hlgl::Feature::Imgui });
  if (!context) {
    fmt::println("HLGL context creation failed.");
    return 1;
  }

  // Loop until the window is closed.
  while (!glfwWindowShouldClose(window)) {

    glfwPollEvents();
    context.imguiNewFrame();

    ImGui::ShowDemoWindow();
    ImGui::Render();

    // Begin the frame.  When the Frame object is destroyed at the end of this scope, the frame will be presented to the screen.
    if (hlgl::Frame frame = context.beginFrame(); frame)
    {
      // Begin a drawing pass.
      // Although we aren't drawing anything, it's neccessary to clear the screen.
      frame.beginDrawing({hlgl::AttachColor{
        .texture = frame.getSwapchainTexture(),
        .clear = hlgl::ColorRGBAf{0.5f, 0.0f, 0.5f, 1.0f}
        }});
    }
  }

  return 0;
}