#include <wcgl/wcgl.h>
#include <GLFW/glfw3.h>
#include <fmt/format.h>
#include <imgui.h>


int main(int, char**) {
  // Create the window.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(800, 600, "Hello ImGui WCGL", nullptr, nullptr);
  if (!window) {
    fmt::println("Window creation failed.");
    return 1;
  }

  // Create the WCGL context.
  wcgl::Context context(wcgl::ContextParams{
    .pWindow = window,
    .fnDebugCallback = [](wcgl::DebugSeverity severity, std::string_view msg){fmt::println("[WCGL] {}", msg);},
    .requiredFeatures = wcgl::Feature::Validation | wcgl::Feature::Imgui });
  if (!context) {
    fmt::println("WCGL context creation failed.");
    return 1;
  }

  // Loop until the window is closed.
  while (!glfwWindowShouldClose(window)) {

    glfwPollEvents();
    context.imguiNewFrame();

    ImGui::ShowDemoWindow();
    ImGui::Render();

    // Begin the frame.  When the Frame object is destroyed at the end of this scope, the frame will be presented to the screen.
    if (wcgl::Frame frame = context.beginFrame(); frame)
    {
      // Begin a drawing pass.
      // Although we aren't drawing anything, it's neccessary to clear the screen.
      frame.beginDrawing({wcgl::AttachColor{
        .texture = frame.getSwapchainTexture(),
        .clear = wcgl::ColorRGBAf{0.5f, 0.0f, 0.5f, 1.0f}
        }});
    }
  }

  return 0;
}