#include <wcgl/wcgl.h>
#include <GLFW/glfw3.h>
#include <fmt/base.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

const char* gradient_color_comp = R"ComputeShader(
#version 460
//#extension GL_KHR_vulkan_glsl: enable

layout (local_size_x = 16, local_size_y = 16) in;

layout (rgba16f,set = 0, binding = 0) uniform image2D image;

layout (push_constant) uniform constants
{
  vec4 data0;
  vec4 data1;
  vec4 data2;
  vec4 data3;
} pushConstants;

void main()
{
  ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(image);

  vec4 topColor = pushConstants.data0;
  vec4 bottomColor = pushConstants.data1;

  if (texelCoord.x < size.x && texelCoord.y < size.y) {
  float blend = float(texelCoord.y)/(size.y);
  imageStore(image, texelCoord, mix(topColor, bottomColor, blend));
  }
}
)ComputeShader";

const char* sky_comp = R"ComputeShader(
#version 450
//#extension GL_KHR_vulkan_glsl: enable
layout (local_size_x = 16, local_size_y = 16) in;
layout(rgba8,set = 0, binding = 0) uniform image2D image;

// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

//push constants block
layout( push_constant ) uniform constants
{
 vec4 data1;
 vec4 data2;
 vec4 data3;
 vec4 data4;
} PushConstants;

// Return random noise in the range [0.0, 1.0], as a function of x.
float Noise2d( in vec2 x )
{
    float xhash = cos( x.x * 37.0 );
    float yhash = cos( x.y * 57.0 );
    return fract( 415.92653 * ( xhash + yhash ) );
}

// Convert Noise2d() into a "star field" by stomping everthing below fThreshhold to zero.
float NoisyStarField( in vec2 vSamplePos, float fThreshhold )
{
    float StarVal = Noise2d( vSamplePos );
    if ( StarVal >= fThreshhold )
        StarVal = pow( (StarVal - fThreshhold)/(1.0 - fThreshhold), 6.0 );
    else
        StarVal = 0.0;
    return StarVal;
}

// Stabilize NoisyStarField() by only sampling at integer values.
float StableStarField( in vec2 vSamplePos, float fThreshhold )
{
    // Linear interpolation between four samples.
    // Note: This approach has some visual artifacts.
    // There must be a better way to "anti alias" the star field.
    float fractX = fract( vSamplePos.x );
    float fractY = fract( vSamplePos.y );
    vec2 floorSample = floor( vSamplePos );    
    float v1 = NoisyStarField( floorSample, fThreshhold );
    float v2 = NoisyStarField( floorSample + vec2( 0.0, 1.0 ), fThreshhold );
    float v3 = NoisyStarField( floorSample + vec2( 1.0, 0.0 ), fThreshhold );
    float v4 = NoisyStarField( floorSample + vec2( 1.0, 1.0 ), fThreshhold );

    float StarVal =   v1 * ( 1.0 - fractX ) * ( 1.0 - fractY )
              + v2 * ( 1.0 - fractX ) * fractY
              + v3 * fractX * ( 1.0 - fractY )
              + v4 * fractX * fractY;
  return StarVal;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 iResolution = imageSize(image);
  // Sky Background Color
  //vec3 vColor = vec3( 0.1, 0.2, 0.4 ) * fragCoord.y / iResolution.y;
    vec3 vColor = PushConstants.data1.xyz * fragCoord.y / iResolution.y;

    // Note: Choose fThreshhold in the range [0.99, 0.9999].
    // Higher values (i.e., closer to one) yield a sparser starfield.
    float StarFieldThreshhold = PushConstants.data1.w;//0.97;

    // Stars with a slow crawl.
    float xRate = 0.2;
    float yRate = -0.06;
    vec2 vSamplePos = fragCoord.xy + vec2( xRate * float( 1 ), yRate * float( 1 ) );
  float StarVal = StableStarField( vSamplePos, StarFieldThreshhold );
    vColor += vec3( StarVal );
  
  fragColor = vec4(vColor, 1.0);
}

void main() 
{
  vec4 value = vec4(0.0, 0.0, 0.0, 1.0);
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(image);
    if(texelCoord.x < size.x && texelCoord.y < size.y)
    {
        vec4 color;
        mainImage(color,texelCoord);
    
        imageStore(image, texelCoord, color);
    }   
}
)ComputeShader";

const char* object_vert = R"VertexShader(
#version 450
#extension GL_EXT_buffer_reference : require
//#extension GL_ARB_separate_shader_objects : enable

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
  mat4 worldMatrix;
  VertexBuffer vertexBuffer;
} pushConstants;

void main() {
  Vertex vert = pushConstants.vertexBuffer.vertices[gl_VertexIndex];

  gl_Position = pushConstants.worldMatrix * vec4(vert.position, 1);
  outColor = vert.color.rgb;
  outTexCoord = vec2(vert.u, vert.v);
}
)VertexShader";

const char* object_frag = R"FragmentShader(
#version 450
//#extension GL_ARB_separate_shader_objects : enable

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
  GLFWwindow* window = glfwCreateWindow(800, 600, "Hello Compute WCGL", nullptr, nullptr);
  if (!window) {
    fmt::println("Window creation failed.");
    return 1;
  }

  // Create the WCGL context.
  wcgl::Context context(wcgl::ContextParams{
    .pWindow = window,
    .fnDebugCallback = [](wcgl::DebugSeverity severity, std::string_view msg) {fmt::println("[WCGL] {}", msg); },
    .requiredFeatures = wcgl::Feature::Validation | wcgl::Feature::Imgui | wcgl::Feature::BufferDeviceAddress });
  if (!context) {
    fmt::println("WCGL context creation failed.");
    return 1;
  }

  // Draw the first frame so the screen will be black while the shaders are loading.
  if (wcgl::Frame frame = context.beginFrame(); frame) {
    frame.beginDrawing({wcgl::AttachColor{
      .texture = frame.getSwapchainTexture(),
      .clear = wcgl::ColorRGBAf{0.0f, 0.0f, 0.0f, 1.0f}
      }});
  }

  // Create the texture we'll draw to.
  wcgl::Texture drawTarget(context, wcgl::TextureParams{
    .bMatchDisplaySize = true,
    .eFormat = wcgl::Format::RGBA16f,
    .eUsage = wcgl::TextureUsage::Framebuffer | wcgl::TextureUsage::Storage,
    .sDebugName = "drawTarget" });

  // The push constant that we'll send to the compute shader.
  struct PushConst {
    glm::vec4 data0{0.05,0,0.1,0.985}, data1{0.9,0.8,1,1}, data2{}, data3{};
  } pushConst;
  int whichEffect{0};
  std::array effectNames{"gradient", "sky"};

  // Create the pipeline for the compute shader.
  std::array<wcgl::ComputePipeline, 2> computeEffects{
    wcgl::ComputePipeline(context, {.computeShader = {.sName = "gradient_color.comp", .sGlsl = gradient_color_comp}}),
    wcgl::ComputePipeline(context, {.computeShader = {.sName = "sky.comp", .sGlsl = sky_comp}})
  };

  // Create the pipeline for the graphics shaders.
  wcgl::GraphicsPipeline graphicsPipeline(context, wcgl::GraphicsPipelineParams{
    .vertexShader   = {.sName = "object.vert", .sGlsl = object_vert },
    .fragmentShader = {.sName = "object.frag", .sGlsl = object_frag },
    .colorAttachments = {wcgl::ColorAttachment{.format = wcgl::Format::RGBA16f}},
  });
  if (!graphicsPipeline) {
    fmt::println("WCGL graphics pipeline creation failed.");
    return 1;
  }

  // Define some vertices and create a vertex buffer.
  struct Vertex {
    glm::vec3 position{0.0f,0.0f,0.0f}; float u{0.0f};
    glm::vec3 normal  {0.0f,0.0f,0.0f}; float v{0.0f};
    glm::vec4 color{1.0f,1.0f,1.0f,1.0f};
  };
  std::array vertices{
    Vertex{.position = {0.5, -0.5, 0},  .color = {0, 0, 0, 1}},
    Vertex{.position = {0.5, 0.5, 0},   .color = {0, 0, 1, 1}},
    Vertex{.position = {-0.5, -0.5, 0}, .color = {1, 0, 0, 1}},
    Vertex{.position = {-0.5, 0.5, 0},  .color = {0, 1, 0, 1}}
  };
  wcgl::Buffer vertexBuffer(context, wcgl::BufferParams{
    .usage = wcgl::BufferUsage::Storage | wcgl::BufferUsage::DeviceAddressable,
    .iSize = sizeof(Vertex) * vertices.size(),
    .pData = vertices.data() });

  // Define some indices and create an index buffer.
  std::array<uint32_t, 6> indices{0, 2, 1, 2, 3, 1};
  wcgl::Buffer indexBuffer(context, wcgl::BufferParams{
    .usage = wcgl::BufferUsage::Index,
    .iIndexSize = sizeof(uint32_t),
    .iSize = sizeof(uint32_t) * indices.size(),
    .pData = indices.data() });

  struct DrawPushConsts {
    glm::mat4 worldMatrix{};
    wcgl::DeviceAddress vertexBuffer{0};
  } drawPushConsts;
  drawPushConsts.worldMatrix = glm::identity<glm::mat4>();
  drawPushConsts.vertexBuffer = vertexBuffer.getDeviceAddress();

  // Loop until the window is closed.
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    context.imguiNewFrame();
    if (ImGui::Begin("Background")) {
      ImGui::Text("Selected effect: %s", effectNames [whichEffect]);
      ImGui::SliderInt("Effect Index", &whichEffect, 0, (int)effectNames.size()-1);
      ImGui::InputFloat4("data0", (float*)&pushConst.data0);
      ImGui::InputFloat4("data1", (float*)&pushConst.data1);
      ImGui::InputFloat4("data2", (float*)&pushConst.data2);
      ImGui::InputFloat4("data3", (float*)&pushConst.data3);
    } ImGui::End();
    ImGui::Render();

    // Begin the frame.  When the Frame object is destroyed at the end of this scope, the frame will be presented to the screen.
    if (wcgl::Frame frame = context.beginFrame(); frame) {
      
      // Use the selected compute shader to draw the background.
      frame.bindPipeline(computeEffects[whichEffect]);
      frame.pushBindings(0, {wcgl::WriteTexture{&drawTarget, 0}});
      frame.pushConstants(&pushConst, sizeof(PushConst));
      auto [w, h] = context.getDisplaySize();
      frame.dispatch((uint32_t)std::ceil(w/16.0f), (uint32_t)std::ceil(h/16.0f), 1);

      // Draw some geometry on top of it.
      frame.beginDrawing({wcgl::AttachColor{
        .texture = &drawTarget,
        .clear = std::nullopt }});
      frame.bindPipeline(graphicsPipeline);
      frame.pushConstants(&drawPushConsts, sizeof(DrawPushConsts));
      frame.drawIndexed(&indexBuffer, 6);

      // Blit our render texture to the swapchain.
      frame.blit(*frame.getSwapchainTexture(), drawTarget, {}, {});
    }
  }

  return 0;
}