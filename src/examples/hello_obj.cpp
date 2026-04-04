#include <hlgl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include <print>

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

  struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
  };

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
  hlgl::Buffer mesh(hlgl::BufferParams{
    .usage = hlgl::BufferUsage::Vertex | hlgl::BufferUsage::Index,
    .data = {{.size = vBufSize, .ptr = vertices.data()}, {.size = iBufSize, .ptr = indices.data()}},
    .indexSize = 2
  });

  struct ShaderData {
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 model[3];
    glm::vec4 lightPos{0.0f, -10.0f, 10.0f, 0.0f};
    uint32_t selected{1};
  } shaderData{};

  hlgl::Buffer uniforms(hlgl::BufferParams{
    .usage = hlgl::BufferUsage::DeviceAddressable | hlgl::BufferUsage::Uniform | hlgl::BufferUsage::Updateable,
    .data = {{.size = sizeof(ShaderData), .ptr = &shaderData}}
  });

  return 0;
}