#include <hlgl/core/frame.h>
#include <hlgl/plus/mesh.h>
#include <hlgl/plus/vertex.h>

#include <iostream>
#include <string>
#include <unordered_map>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fmt/format.h>
//#include <glm/gtx/quaternion.hpp>
#include <stb_image.h>


namespace {

std::unordered_map<std::string, std::vector<std::shared_ptr<hlgl::Mesh>>> loadedMeshes_s;

} // namespace <anon>

hlgl::Mesh::Mesh(Mesh&& other)
: name_(std::move(other.name_)),
  indexBuffer_(std::move(other.indexBuffer_)),
  vertexBuffer_(std::move(other.vertexBuffer_)),
  subMeshes_(std::move(other.subMeshes_))
{}


std::vector<hlgl::Mesh> hlgl::Mesh::loadGltf(const hlgl::Context& context, const std::filesystem::path& filepath) {
  //if (loadedMeshes_s.count(filepath.string()))
  //  return loadedMeshes_s [filepath.string()];

  auto data = fastgltf::GltfDataBuffer::FromPath(filepath);
  if (data.error() != fastgltf::Error::None) {
    fmt::println("Failed to load file '{}'", filepath.string());
    return {};
  }

  fastgltf::Parser parser;
  auto asset = parser.loadGltf(data.get(), filepath.parent_path(), fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers);
  if (auto error = asset.error(); error != fastgltf::Error::None) {
    fmt::println("Failed to load file '{}'", filepath.string());
    return {};
  }
  auto& gltf = asset.get();

  std::vector<Mesh> meshes;
  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;

  for (const fastgltf::Mesh& mesh : gltf.meshes) {
    meshes.emplace_back(context);
    Mesh& newMesh = meshes.back();
    newMesh.name_ = mesh.name;

    indices.clear();
    vertices.clear();

    for (const auto& p : mesh.primitives) {
      SubMesh subMesh;
      subMesh.start = (uint32_t)indices.size();
      subMesh.count = (uint32_t)gltf.accessors [p.indicesAccessor.value()].count;
      size_t initialVertex = vertices.size();

      // Load indices.
      {
        fastgltf::Accessor& indexAccessor = gltf.accessors [p.indicesAccessor.value()];
        indices.reserve(indices.size() + indexAccessor.count);

        fastgltf::iterateAccessor<uint32_t>(gltf, indexAccessor, [&](uint32_t i) {
          indices.push_back(i + (uint32_t)initialVertex);
        });
      }

      // Load vertex positions.
      {
        fastgltf::Accessor& posAccessor = gltf.accessors [p.findAttribute("POSITION")->accessorIndex];
        vertices.resize(vertices.size() + posAccessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor, [&](glm::vec3 v, size_t index) {
          Vertex newVertex;
          newVertex.position = v;
          newVertex.normal ={1,0,0};
          newVertex.color ={1.f, 1.f, 1.f, 1.f};
          newVertex.u = 0;
          newVertex.v = 0;
          vertices [initialVertex + index] = newVertex;
        });
      }

      // Load vertex normals.
      auto normals = p.findAttribute("NORMAL");
      if (normals != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex], [&](glm::vec3 v, size_t index) {
          vertices [initialVertex + index].normal = v;
        });
      }

      // Load vertex texture coordinates.
      auto uvs = p.findAttribute("TEXCOORD_0");
      if (uvs != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors [uvs->accessorIndex], [&](glm::vec2 v, size_t index) {
          vertices [initialVertex + index].u = v.x;
          vertices [initialVertex + index].v = v.y;
        });
      }

      // Load vertex colors.
      auto colors = p.findAttribute("COLOR_0");
      if (colors != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors [colors->accessorIndex], [&](glm::vec4 v, size_t index) {
          vertices [initialVertex + index].color = v;
        });
      }

      newMesh.subMeshes_.push_back(subMesh);
    }

    // Use vertex normals to override vertex colors for debugging.
    if (true) {
      for (Vertex& v : vertices) {
        v.color = glm::vec4{v.normal, 1.f};
      }
    }

    newMesh.indexBuffer_.Construct({
      .usage = hlgl::BufferUsage::Index,
      .iIndexSize = sizeof(uint32_t),
      .iSize = indices.size() * sizeof(uint32_t),
      .pData = indices.data()});

    newMesh.vertexBuffer_.Construct({
      .usage = hlgl::BufferUsage::Storage | hlgl::BufferUsage::Vertex | hlgl::BufferUsage::DeviceAddressable,
      .iSize = vertices.size() * sizeof(Vertex),
      .pData = vertices.data()});

  }

  //loadedMeshes_s [filepath.string()] = meshes;
  return std::move(meshes);
}

void hlgl::Mesh::draw(hlgl::Frame& frame) {
  for (auto& subMesh : subMeshes_) {
    frame.drawIndexed(&indexBuffer_, subMesh.count, 1, subMesh.start, 0, 0);
  }
}
