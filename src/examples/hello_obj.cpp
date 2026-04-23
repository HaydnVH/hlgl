#include <hlgl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <tiny_obj_loader.h>

#include <iostream>

const char* shader_slang = R"(
  /* Copyright (c) 2025-2026, Sascha Willems
  *
  * SPDX-License-Identifier: MIT
  *
  */

  struct VSInput {
    float3 Pos;     float pad0;
    float3 Normal;  float pad1;
    float2 UV;
  };

  [vk::binding(0,1)]
  Sampler2D textures[];

  struct ShaderData {
    float4x4 projection;
    float4x4 view;
    float4x4 model[3];
    float4 lightPos;
    uint32_t material[3];
    uint32_t selected;
  };

  struct VSOutput {
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 UV;
    float3 Factor;
    float3 LightVec;
    float3 ViewVec;
    uint32_t materialIndex;
  };

  [shader("vertex")]
  VSOutput main(uniform ShaderData* shaderData, uniform VSInput* vertices, uniform uint16_t* indices, uint vertIndex : SV_VertexID, uint instIndex : SV_VulkanInstanceID) {
      VSInput input = vertices[indices[vertIndex]];
      VSOutput output;
      float4x4 modelMat = shaderData->model[instIndex];
      output.Normal = mul((float3x3)mul(shaderData->view, modelMat), input.Normal);
      output.UV = input.UV;
      output.Pos = mul(shaderData->projection, mul(shaderData->view, mul(modelMat, float4(input.Pos.xyz, 1.0))));
      output.Factor = (shaderData->selected == instIndex ? 3.0f : 1.0f);
      output.materialIndex = shaderData->material[instIndex];
      // Calculate view vectors required for lighting
      float4 fragPos = mul(mul(shaderData->view, modelMat), float4(input.Pos.xyz, 1.0));
      output.LightVec = shaderData->lightPos.xyz - fragPos.xyz;
      output.ViewVec = -fragPos.xyz;
      return output;
  }

  [shader("fragment")]
  float4 main(VSOutput input) {
      // Phong lighting
      float3 N = normalize(input.Normal);
      float3 L = normalize(input.LightVec);
      float3 V = normalize(input.ViewVec);
      float3 R = reflect(-L, N);
      float3 diffuse = max(dot(N, L), 0.0025);
      float3 specular = pow(max(dot(R, V), 0.0), 16.0) * 0.75;
      // Sample from texture
      float3 color = textures[NonUniformResourceIndex(input.materialIndex)].Sample(input.UV).rgb * input.Factor;
      return float4(diffuse * color.rgb + specular, 1.0);
  }
)";

int main(int, char**) {
  
  // Create the window.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(1920, 1080, "Hello Obj HLGL", nullptr, nullptr);
  if (!window) {
    std::cerr << "Window creation failed.\n";
    return 1;
  }

  // Create the HLGL context.
  if (!hlgl::initContext(hlgl::InitContextParams{
    .window = window,
    .debugCallback = [](hlgl::DebugSeverity severity, std::string_view message){std::cout << "[HLGL] " << message << std::endl;},
    .requiredFeatures = hlgl::Feature::Validation}))
  {
    std::cerr << "HLGL context creation failed.\n";
    return 1;
  }
  else {

    hlgl::Texture depthBuffer(hlgl::Texture::CreateParams{
      .usage = hlgl::TextureUsage::Framebuffer | hlgl::TextureUsage::ScreenSize,
      .format = hlgl::ImageFormat::D24S8,
      .debugName = "depthBuffer"
    });

    struct Vertex {
      glm::vec3 pos;    float pad0;
      glm::vec3 normal; float pad1;
      glm::vec2 uv;
    };

    // Load the obj file.
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, nullptr, "../../assets/models/suzanne.obj");

    size_t indexCount {shapes[0].mesh.indices.size()};
    std::vector<Vertex> vertices{};
    std::vector<uint16_t> indices{};
    for (tinyobj::index_t& index : shapes[0].mesh.indices) {
      Vertex v{
        .pos = { attrib.vertices[index.vertex_index*3], -attrib.vertices[index.vertex_index*3 +1], attrib.vertices[index.vertex_index*3 +2] },
        .normal = { attrib.normals[index.normal_index*3], -attrib.normals[index.normal_index*3 +1], attrib.normals[index.normal_index*3 +2] },
        .uv = { attrib.texcoords[index.texcoord_index*2], 1.0f - attrib.texcoords[index.texcoord_index*2 +1] }
      };
      vertices.push_back(v);
      indices.push_back(indices.size());
    }

    size_t vBufSize { sizeof(Vertex) * vertices.size() };
    size_t iBufSize { sizeof(uint16_t) * indices.size() };
    hlgl::Buffer mesh(hlgl::Buffer::CreateParams{
      .usage = hlgl::BufferUsage::Vertex | hlgl::BufferUsage::Index | hlgl::BufferUsage::DeviceAddressable,
      .data = {{.ptr = vertices.data(), .size = vBufSize}, {.ptr = indices.data(), .size = iBufSize}},
      .debugName = "suzanne.obj"
    });

    // Load the ktx textures.
    hlgl::Texture tex0(hlgl::Texture::LoadKtxParams{.filename = "../../assets/textures/suzanne0.ktx"});
    hlgl::Texture tex1(hlgl::Texture::LoadKtxParams{.filename = "../../assets/textures/suzanne1.ktx"});
    hlgl::Texture tex2(hlgl::Texture::LoadKtxParams{.filename = "../../assets/textures/suzanne2.ktx"});

    struct ShaderData {
      glm::mat4 proj;
      glm::mat4 view;
      glm::mat4 model[3];
      glm::vec4 lightPos{0.0f, -10.0f, 10.0f, 0.0f};
      uint32_t material[3];
      uint32_t selected{1};
    } shaderData{};

    hlgl::Buffer uniforms(hlgl::Buffer::CreateParams{
      .usage = hlgl::BufferUsage::DeviceAddressable | hlgl::BufferUsage::Uniform | hlgl::BufferUsage::Updateable,
      .size = sizeof(ShaderData),
      .debugName = "uniforms"
    });

    hlgl::Shader shader(hlgl::Shader::CreateParams{.src = shader_slang, .debugName = "shader.slang"});
    hlgl::Pipeline pipeline(hlgl::Pipeline::GraphicsParams{
      .vertShader = {.shader = &shader},
      .fragShader = {.shader = &shader},
      .colorAttachments = {hlgl::ColorAttachmentInfo{.format = hlgl::getDisplayFormat()}},
      .depthAttachment = hlgl::DepthAttachmentInfo{.format = hlgl::ImageFormat::D24S8}
    });

    struct PushConstants {
      hlgl::DeviceAddress shaderData;
      hlgl::DeviceAddress vertices;
      hlgl::DeviceAddress indices;
    } pushConstants{};

    // Loop until the window is closed.
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      glm::vec3 camPos{0.0f,0.0f,-10.0f};

      // Begin the frame.
      hlgl::Result result = hlgl::beginFrame();
      if (result == hlgl::Result::Error)
        break;
      else if (result == hlgl::Result::Success)
      {
        hlgl::bindPipeline(&pipeline);

        shaderData.proj = glm::perspective(glm::radians(45.0f), hlgl::getDisplayAspectRatio(), 0.1f, 32.0f);
        shaderData.view = glm::translate(glm::mat4(1.0f), camPos);
        shaderData.model[0] = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, 0.0f, 0.0f));
        shaderData.model[1] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        shaderData.model[2] = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f));
        shaderData.material[0] = tex0.getSamplerIndex();
        shaderData.material[1] = tex1.getSamplerIndex();
        shaderData.material[2] = tex2.getSamplerIndex();

        uniforms.updateData(&shaderData, sizeof(ShaderData), 0);

        hlgl::beginDrawing(
          {hlgl::ColorAttachment{.texture = hlgl::getFrameSwapchainImage(), .clear = hlgl::ColorRGBAf{0.0f, 0.0f, 0.2f, 1.0f}}},
          hlgl::DepthAttachment{.texture = &depthBuffer, .clear = hlgl::DepthStencilClearVal{1.0f, 0}});
        
        pushConstants.shaderData = uniforms.getAddress();
        pushConstants.vertices = mesh.getAddress();
        pushConstants.indices = pushConstants.vertices + vBufSize;
        hlgl::pushConstants(&pushConstants, sizeof(PushConstants));
        
        hlgl::draw(indexCount, 3);
        hlgl::endFrame();
      }
    } 
  }
  hlgl::shutdownContext();
  return 0;
}