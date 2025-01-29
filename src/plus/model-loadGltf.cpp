#include <hlgl/core/frame.h>
#include <hlgl/plus/assetcache.h>
#include <hlgl/plus/model.h>
#include "../core/debug.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fmt/core.h>
//#include <glm/gtx/quaternion.hpp>

void hlgl::Model::importGltf(hlgl::Context& context, hlgl::AssetCache& assetCache, const std::filesystem::path& filePath) {
  
  if (meshes_.size() > 0) {
    debugPrint(DebugSeverity::Warning, "Can't load GLTF file using a Model object that already has loaded data.");
    return;
  }

  debugPrint(DebugSeverity::Info, fmt::format("Loading GLTF file '{}'.", filePath.string()));

  auto data = fastgltf::GltfDataBuffer::FromPath(filePath);
  if (data.error() != fastgltf::Error::None) {
    debugPrint(DebugSeverity::Error, fmt::format("Failed to load GLTF file '{}'.", filePath.string()));
    return;
  }

  constexpr fastgltf::Options options =
    fastgltf::Options::DontRequireValidAssetMember |
    fastgltf::Options::AllowDouble |
    fastgltf::Options::LoadGLBBuffers |
    fastgltf::Options::LoadExternalBuffers;

  fastgltf::Parser parser {};

  auto asset = parser.loadGltf(data.get(), filePath.parent_path(), options);
  if (auto error = asset.error(); error != fastgltf::Error::None) {
    debugPrint(DebugSeverity::Error, fmt::format("Failed to load GLTF file '{}'.", filePath.string()));
    return;
  }
  fastgltf::Asset& gltf = asset.get();

  // Temporal arrays for the objects used while creating GLTF data.
  std::vector<std::shared_ptr<Texture>> textures;
  std::vector< std::shared_ptr<Material>> materials;

  // Load all textures.
  for (auto& sampler : gltf.images) {
    textures.push_back(assetCache.loadTexture("hlgl::textures/missing"));
  }

  // Load all materials.
  for (auto& material : gltf.materials) {
    auto newMat = assetCache.loadMaterial(fmt::format("{}:{}", filePath, material.name));
    materials.push_back(newMat);

    glm::vec4 constColor;
    constColor.x = material.pbrData.baseColorFactor[0];
    constColor.y = material.pbrData.baseColorFactor[1];
    constColor.z = material.pbrData.baseColorFactor[2];
    constColor.w = material.pbrData.baseColorFactor[3];
    glm::vec3 constPbr;
    constPbr.x = material.pbrData.metallicFactor;
    constPbr.y = material.pbrData.roughnessFactor;
    // TODO: Write material parameters to buffer

    // Choose between alpha blended or opaque pipeline.
    if (material.alphaMode == fastgltf::AlphaMode::Blend)
      newMat->pipeline = assetCache.loadPipeline("hlgl::pipelines/pbr-blendAlpha");
    else
      newMat->pipeline = assetCache.loadPipeline("hlgl::pipelines/pbr-opaque");

    // Grab the base color texture if one is present, otherwise use 'white'.
    if (material.pbrData.baseColorTexture.has_value())
      newMat->texBaseColor = textures[gltf.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value()];
    else
      newMat->texBaseColor = assetCache.loadTexture("hlgl::textures/white");

    // Grab the normal texture if one is present, otherwise use 'gray'.
    if (material.normalTexture.has_value())
      newMat->texNormal = textures[gltf.textures[material.normalTexture.value().textureIndex].imageIndex.value()];
    else
      newMat->texNormal = assetCache.loadTexture("hlgl::textures/gray");
    
    // Grab the Occlusion-Roughness-Metallic texture if one is present, otherwise use 'white'.
    if (material.packedOcclusionRoughnessMetallicTextures && 
        material.packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture.has_value())
      newMat->texORM = textures[gltf.textures[material.packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture.value().textureIndex].imageIndex.value()];
    else
      newMat->texORM = assetCache.loadTexture("hlgl::textures/white");

    // Grab the emissive texture if one is present, otherwise use 'black'.
    if (material.emissiveTexture.has_value())
      newMat->texEmissive = textures[gltf.textures[material.emissiveTexture.value().textureIndex].imageIndex.value()];
    else
      newMat->texEmissive = assetCache.loadTexture("hlgl::textures/black");
  }
}

/*
void hlgl::Model::importGltf(hlgl::Context& context, hlgl::AssetCache& assetCache, const std::filesystem::path& filePath) {

  if (meshes_.size() > 0) {
    debugPrint(DebugSeverity::Warning, "Can't load GLtf file using a Model object that's already loaded something.");
    return;
  }

  Model result;

  auto data = fastgltf::GltfDataBuffer::FromPath(filePath);
  if (data.error() != fastgltf::Error::None) {
    debugPrint(DebugSeverity::Error, fmt::format("Failed to load file '{}'", filePath.string()));
    return;
  }

  fastgltf::Parser parser;
  auto asset = parser.loadGltf(data.get(), filePath.parent_path(), fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers);
  if (auto error = asset.error(); error != fastgltf::Error::None) {
    debugPrint(DebugSeverity::Error, fmt::format("Failed to load file '{}'", filePath.string()));
    return;
  }
  auto& gltf = asset.get();

  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;

  for (const fastgltf::Mesh& mesh : gltf.meshes) {
    auto [it, inserted] = meshes_.insert(std::make_pair(static_cast<std::string>(mesh.name), Mesh(context)));
    if (!inserted) continue;
    Mesh& newMesh = it->second;

    indices.clear();
    vertices.clear();

    for (const auto& p : mesh.primitives) {
      Mesh::SubMesh subMesh;
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
      .pData = indices.data(),
      .sDebugName = fmt::format("{}[{}].indexBuffer", filePath.filename().string(), mesh.name).c_str()});

    newMesh.vertexBuffer_.Construct({
      .usage = hlgl::BufferUsage::Storage | hlgl::BufferUsage::DeviceAddressable,
      .iSize = vertices.size() * sizeof(Vertex),
      .pData = vertices.data(),
      .sDebugName = fmt::format("{}[{}].vertexBuffer", filePath.filename().string(), mesh.name).c_str()});
  }
}
*/
