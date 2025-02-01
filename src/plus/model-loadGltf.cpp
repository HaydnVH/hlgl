#include <hlgl/hlgl-core.h>
#include <hlgl/plus/assetcache.h>
#include <hlgl/plus/model.h>
#include "../core/debug.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fmt/core.h>
#include <glm/gtx/quaternion.hpp>
#include <variant>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void hlgl::Model::importGltf(hlgl::Context& context, hlgl::AssetCache& assetCache, const std::filesystem::path& filePath) {
  
  if (allMeshes_.size() > 0) {
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

  // Temporary arrays for the objects used while creating GLTF data.
  std::vector<std::shared_ptr<Texture>> textures;
  std::vector<std::shared_ptr<Material>> materials;
  //std::vector<std::unique_ptr<Mesh>> meshes;
  //std::vector<std::unique_ptr<Node>> nodes;

  // Load all textures.
  for (size_t i {0}; i < gltf.images.size(); ++i) {
    auto& image {gltf.images[i]};

    int width {0}, height {0}, numChannels {0};

    std::shared_ptr<Texture> texture {nullptr};

    std::visit(fastgltf::visitor {
      [](auto&) {},
      [&](fastgltf::sources::URI& texturePath) {
        if (texturePath.fileByteOffset != 0 || !texturePath.uri.isLocalPath())
          return;
        const std::string path(texturePath.uri.path().begin(), texturePath.uri.path().end());
        uint8_t* data = stbi_load(path.c_str(), &width, &height, &numChannels, 4);
        if (data) {
          std::string texName = fmt::format("{}:image[{}]", filePath.string(), i);
          texture = assetCache.loadTexture(texName, TextureParams{
            .iWidth = (uint32_t)width,
            .iHeight = (uint32_t)height,
            .eFormat = Format::RGBA8i,
            .eFiltering = FilterMode::Linear,
            .usage = TextureUsage::Sampler,
            .pData = data,
            });
          stbi_image_free(data);
        }},
      [&](fastgltf::sources::Vector& vector) {
        uint8_t* data = stbi_load_from_memory((const uint8_t*)vector.bytes.data(), static_cast<int>(vector.bytes.size()), &width, &height, &numChannels, 4);
        if (data) {
          std::string texName = fmt::format("{}:image[{}]", filePath.string(), i);
          texture =  assetCache.loadTexture(texName, TextureParams{
            .iWidth = (uint32_t)width,
            .iHeight = (uint32_t)height,
            .eFormat = Format::RGBA8i,
            .eFiltering = FilterMode::Linear,
            .usage = TextureUsage::Sampler,
            .pData = data,
            });
          stbi_image_free(data);
        }
      },
      [&](fastgltf::sources::BufferView& view) {
        fastgltf::BufferView& bufferView = asset->bufferViews[view.bufferViewIndex];
        fastgltf::Buffer& buffer = asset->buffers[bufferView.bufferIndex];
        std::visit(fastgltf::visitor{
          [](auto&) {},
          [&](fastgltf::sources::Array& vector) {
            uint8_t* data = stbi_load_from_memory((const uint8_t*)vector.bytes.data() + bufferView.byteOffset, bufferView.byteLength, &width, &height, &numChannels, 4);
            if (data) {
              std::string texName = fmt::format("{}:image[{}]", filePath.string(), i);
              texture = assetCache.loadTexture(texName, TextureParams{
                .iWidth = (uint32_t)width,
                .iHeight = (uint32_t)height,
                .eFormat = Format::RGBA8i,
                .eFiltering = FilterMode::Linear,
                .usage = TextureUsage::Sampler,
                .pData = data,
                });
              stbi_image_free(data);
            }
          },
          [&](fastgltf::sources::Vector& vector) {
            uint8_t* data = stbi_load_from_memory((const uint8_t*)vector.bytes.data() + bufferView.byteOffset, bufferView.byteLength, &width, &height, &numChannels, 4);
            if (data) {
              std::string texName = fmt::format("{}:image[{}]", filePath.string(), i);
              texture = assetCache.loadTexture(texName, TextureParams{
                .iWidth = (uint32_t)width,
                .iHeight = (uint32_t)height,
                .eFormat = Format::RGBA8i,
                .eFiltering = FilterMode::Linear,
                .usage = TextureUsage::Sampler,
                .pData = data,
                });
              stbi_image_free(data);
            }
          },
        }, buffer.data);
      },
    }, image.data);

    if (texture)
      textures.push_back(texture);
    else
      textures.push_back(assetCache.loadTexture("hlgl::textures/missing"));
  }

  // Load all materials.
  for (fastgltf::Material& material : gltf.materials) {
    auto newMat = assetCache.loadMaterial(fmt::format("{}:{}", filePath.string(), material.name.c_str()));
    materials.push_back(newMat);

    // Choose between alpha blended or opaque pipeline.
    if (material.alphaMode == fastgltf::AlphaMode::Blend)
      newMat->pipeline = assetCache.loadPipeline("hlgl::pipelines/pbr-blendAlpha");
    else
      newMat->pipeline = assetCache.loadPipeline("hlgl::pipelines/pbr-opaque");

    // Grab the base color texture if one is present, otherwise use 'white'.
    if (material.pbrData.baseColorTexture.has_value())
      newMat->textures.baseColor = textures[gltf.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value()];
    else
      newMat->textures.baseColor = assetCache.loadTexture("hlgl::textures/white");
    newMat->uniforms.baseColor = glm::vec4{
      material.pbrData.baseColorFactor[0],
      material.pbrData.baseColorFactor[1],
      material.pbrData.baseColorFactor[2],
      material.pbrData.baseColorFactor[3]};

    // Grab the normal texture if one is present, otherwise use 'gray'.
    if (material.normalTexture.has_value())
      newMat->textures.normal = textures[gltf.textures[material.normalTexture.value().textureIndex].imageIndex.value()];
    else
      newMat->textures.normal = assetCache.loadTexture("hlgl::textures/gray");
    
    // Grab the Occlusion-Roughness-Metallic texture if one is present, otherwise use 'white'.
    if (material.packedOcclusionRoughnessMetallicTextures && 
        material.packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture.has_value())
      newMat->textures.occlusionRoughnessMetallic = textures[gltf.textures[material.packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture.value().textureIndex].imageIndex.value()];
    else
      newMat->textures.occlusionRoughnessMetallic = assetCache.loadTexture("hlgl::textures/white");
    newMat->uniforms.roughnessMetallic = glm::vec2{
      material.pbrData.roughnessFactor,
      material.pbrData.metallicFactor};

    // Grab the emissive texture if one is present, otherwise use 'white'.
    if (material.emissiveTexture.has_value())
      newMat->textures.emissive = textures[gltf.textures[material.emissiveTexture.value().textureIndex].imageIndex.value()];
    else
      newMat->textures.emissive = assetCache.loadTexture("hlgl::textures/white");
    newMat->uniforms.emissive = glm::vec4{
      material.emissiveFactor[0],
      material.emissiveFactor[1],
      material.emissiveFactor[2],
      material.emissiveStrength};

    // TODO: Write material uniforms to buffer

    // TODO: Load properties for GLTF material extensions:
    // - KHR_materials_anisotropy
    // - KHR_materials_clearcoat
    // - KHR_materials_diffuse_transmission (release candidate)
    // - KHR_materials_dispersion
    // - KHR_materials_ior
    // - KHR_materials_iridescence
    // - KHR_materials_sheen
    // - KHR_materials_specular
    // - KHR_materials_subsurface (initial draft)
    // - KHR_materials_transmission
    // - KHR_materials_unlit
    // - KHR_materials_variants
    // - KHR_materials_volume
  }

  // Use the same vectors for all meshes so memory doesn't reallocate often.
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  for (size_t i{0}; i < gltf.meshes.size(); ++i) {
    fastgltf::Mesh& gltfMesh = gltf.meshes[i];
    allMeshes_.emplace_back(context);
    Mesh& newMesh = allMeshes_.back();
    meshMap_[static_cast<std::string>(gltfMesh.name)] = i;

    // Clear the mesh arrays for each mesh, for now each mesh gets its own vertex/index buffer.
    vertices.clear();
    indices.clear();

    for (auto&& p : gltfMesh.primitives) {
      Mesh::SubMesh subMesh;
      subMesh.start = (uint32_t)indices.size();
      subMesh.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

      size_t baseVertex = vertices.size();

      // Load indices
      {
        fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
        indices.reserve(indices.size() + indexAccessor.count);
        fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
          [&](std::uint32_t i) {
          indices.push_back(i + baseVertex);
        });
      }

      // Load vertex positions.
      {
        fastgltf::Accessor& posAccessor = gltf.accessors [p.findAttribute("POSITION")->accessorIndex];
        vertices.resize(vertices.size() + posAccessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
          [&](glm::vec3 v, size_t i) {
          Vertex newVertex;
          newVertex.position = v;
          newVertex.normal ={1,0,0};
          newVertex.color ={1.f, 1.f, 1.f, 1.f};
          newVertex.u = 0;
          newVertex.v = 0;
          vertices [i + baseVertex] = newVertex;
        });
      }

      // Load vertex normals.
      auto normals = p.findAttribute("NORMAL");
      if (normals != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex],
          [&](glm::vec3 v, size_t i) {
          vertices [i + baseVertex].normal = v;
        });
      }

      // Load vertex texture coordinates.
      auto uvs = p.findAttribute("TEXCOORD_0");
      if (uvs != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors [uvs->accessorIndex],
          [&](glm::vec2 v, size_t i) {
          vertices [i + baseVertex].u = v.x;
          vertices [i + baseVertex].v = v.y;
        });
      }

      // Load vertex colors.
      auto colors = p.findAttribute("COLOR_0");
      if (colors != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors [colors->accessorIndex],
          [&](glm::vec4 v, size_t i) {
          vertices [i + baseVertex].color = v;
        });
      }

      // Fetch material.
      if (p.materialIndex.has_value())
        subMesh.material = materials[p.materialIndex.value()];
      else
        subMesh.material = materials[0];

      // Save the submesh.
      newMesh.subMeshes_.push_back(subMesh);
    }

    // Upload buffer data to the GPU.
    newMesh.indexBuffer_.Construct({
      .usage = hlgl::BufferUsage::Index,
      .iIndexSize = sizeof(uint32_t),
      .iSize = indices.size() * sizeof(uint32_t),
      .pData = indices.data(),
      .sDebugName = fmt::format("{}[{}].indexBuffer", filePath.filename().string(), gltfMesh.name).c_str()});

    newMesh.vertexBuffer_.Construct({
      .usage = hlgl::BufferUsage::Storage | hlgl::BufferUsage::DeviceAddressable,
      .iSize = vertices.size() * sizeof(Vertex),
      .pData = vertices.data(),
      .sDebugName = fmt::format("{}[{}].vertexBuffer", filePath.filename().string(), gltfMesh.name).c_str()});
  }

  // Load each of the nodes.
  for (fastgltf::Node& node : gltf.nodes) {
    allNodes_.emplace_back();
    std::unique_ptr<Node>& newNode = allNodes_.back();
    nodeMap_[static_cast<std::string>(node.name)] = newNode.get();

    // Load has a mesh node if the node has a mesh,
    if (node.meshIndex.has_value()) {
      auto newMeshNode = std::make_unique<MeshNode>();
      newMeshNode->mesh = &allMeshes_[node.meshIndex.value()];
      newNode = std::move(newMeshNode);
    }
    // otherwise it's an empty node.
    else
      newNode = std::make_unique<Node>();

    // Get the node's local transformation matrix.
    std::visit(fastgltf::visitor{
      [&](fastgltf::math::fmat4x4 matrix) {
      memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
      },
      [&](fastgltf::TRS transform) {
      glm::vec3 translation(transform.translation[0], transform.translation[1], transform.translation[2]);
      glm::quat rotation(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
      glm::vec3 scale(transform.scale[0], transform.scale[1], transform.scale[2]);
      newNode->localTransform =
        glm::translate(glm::identity<glm::mat4>(), translation) * 
        glm::toMat4(rotation) *
        glm::scale(glm::identity<glm::mat4>(), scale);
      }}, node.transform);
  }

  // Go through the nodes again to establish a hierarchy.
  for (size_t i {0}; i < gltf.nodes.size(); ++i) {
    fastgltf::Node& gltfNode = gltf.nodes[i];
    std::unique_ptr<Node>& sceneNode = allNodes_[i];
    for (auto& c : gltfNode.children) {
      sceneNode->children.push_back(allNodes_[c].get());
      allNodes_[c]->parent = sceneNode.get();
    }
  }

  // Find the top nodes with no parents.
  for (auto& node: allNodes_) {
    if (node->parent == nullptr) {
      topNodes_.push_back(node.get());
    }
  }

  // Finally, update each node's world-space transforms.
  updateTransforms();
}