#pragma once
#include <hlgl/hlgl-core.h>
#include <hlgl/plus/vertex.h>

#include <filesystem>
#include <map>
#include <string>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "scene.h"

namespace hlgl {

class AssetCache;
class Material;

class Mesh {
  friend class Model;

  Mesh(const Mesh&) = delete;
  Mesh& operator = (const Mesh&) = delete;

public:
  Mesh(Mesh&& other): matrix_(other.matrix_), vertexBuffer_(std::move(other.vertexBuffer_)), indexBuffer_(std::move(other.indexBuffer_)), subMeshes_(std::move(other.subMeshes_)) {}
  Mesh& operator = (Mesh&& other) noexcept { std::destroy_at(this); std::construct_at(this, std::move(other)); return *this; };
  Mesh(Context& context): vertexBuffer_(context), indexBuffer_(context) {}
  ~Mesh() {};

  bool isValid() const { return (vertexBuffer_.isValid() && indexBuffer_.isValid() && subMeshes_.size() > 0); }
  operator bool () const { return isValid(); }

  const glm::mat4& matrix() const { return matrix_; }
  Buffer* vertexBuffer() { return &vertexBuffer_; }
  Buffer* indexBuffer() { return &indexBuffer_; }

  struct SubMesh {
    uint32_t start {0};
    uint32_t count {0};
    std::shared_ptr<Material> material {nullptr};
  };

  const std::vector<SubMesh>& subMeshes() const { return subMeshes_; }

private:
  glm::mat4 matrix_ {glm::identity<glm::mat4>()};

  Buffer vertexBuffer_;
  Buffer indexBuffer_;

  std::vector<SubMesh> subMeshes_;
};

class Model {
  Model(const Model&) = delete;
  Model& operator = (const Model&) = delete;
public:
  Model() = default;
  Model(Model&& other) noexcept { meshes_ = std::move(other.meshes_); }
  Model& operator = (Model&& other) noexcept { std::destroy_at(this); std::construct_at(this, std::move(other)); return *this; }
  ~Model() {}

  std::map<std::string, Mesh>& meshes() { return meshes_; }
  const std::map<std::string, Mesh>& meshes() const { return meshes_; }
  Mesh* find(const std::string& key) {
    auto it = meshes_.find(key);
    return (it == meshes_.end()) ? nullptr : &it->second;
  };
  Mesh* find(size_t i) {
    auto it = meshes_.begin();
    for (size_t j {0}; (j < i) && (it != meshes_.end()); ++j) { ++it; }
    return (it == meshes_.end()) ? nullptr : &it->second;
  }

  void importGltf(Context& context, AssetCache& assetCache, const std::filesystem::path& filePath);

private:
  std::map<std::string, Mesh> meshes_ {};
  std::map<std::string, std::shared_ptr<Node>> nodes_ {};
  std::vector<std::shared_ptr<Node>> topNodes_ {};
};

/*
class ModelFileData {

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  struct Mesh {

    struct SubMesh {
      uint32_t start {0};
      uint32_t count {0};
      std::string materialName;
    };

    std::vector<SubMesh> subMeshes;
    glm::mat4 matrix;
  };

  std::vector<Mesh> meshes;
};
*/

} // namespace <anon>