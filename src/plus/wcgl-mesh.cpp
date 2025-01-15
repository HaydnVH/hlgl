#include <wcgl/plus/wcgl-mesh.h>
#include <wcgl/plus/wcgl-vertex.h>

#include <iostream>
#include <string>
#include <unordered_map>

//#include <fastgltf/glm_element_traits.hpp>
//#include <fastgltf/tools.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fmt/format.h>
#include <glm/gtx/quaternion.hpp>
#include <stb_image.h>

namespace {

std::unordered_map<std::string, std::shared_ptr<wcgl::Mesh>> loadedMeshes_s;

} // namespace <anon>

std::shared_ptr<wcgl::Mesh> wcgl::Mesh::load(const wcgl::Context& context, const std::filesystem::path& filepath) {
  if (loadedMeshes_s.count(filepath.string()))
    return loadedMeshes_s [filepath.string()];

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

  std::vector<std::shared_ptr<Mesh>> meshes;
  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;

  for (const fastgltf::Mesh& mesh : gltf.meshes) {
    Mesh newMesh(context);
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
          indices.push_back(i + initialVertex);
        });
      }
    }
  }
}