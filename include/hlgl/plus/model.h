#pragma once
#include <hlgl/hlgl-core.h>
#include <hlgl/plus/vertex.h>
#include <hlgl/plus/scene.h>

#include <filesystem>
#include <map>
#include <string>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>


namespace hlgl {

class AssetCache;
class Material;

class Mesh {
  friend class Model;

  Mesh(const Mesh&) = delete;
  Mesh& operator = (const Mesh&) = delete;

public:
  Mesh(Mesh&& other) noexcept: matrix_(other.matrix_), vertexBuffer_(other.vertexBuffer_), indexBuffer_(other.indexBuffer_), subMeshes_(std::move(other.subMeshes_)) {}
  Mesh& operator = (Mesh&& other) noexcept { std::destroy_at(this); std::construct_at(this, std::move(other)); return *this; };
  Mesh() = default;
  ~Mesh() {};

  bool isValid() const { return (vertexBuffer_ && vertexBuffer_->isValid() && indexBuffer_ && indexBuffer_->isValid() && subMeshes_.size() > 0); }
  operator bool () const { return isValid(); }

  const glm::mat4& matrix() const { return matrix_; }
  Buffer* vertexBuffer() { return vertexBuffer_; }
  Buffer* indexBuffer() { return indexBuffer_; }

  struct SubMesh {
    uint32_t start {0};
    uint32_t count {0};
    std::shared_ptr<Material> material {nullptr};
  };

  std::vector<SubMesh>& subMeshes() { return subMeshes_; }

private:
  glm::mat4 matrix_ {glm::identity<glm::mat4>()};

  Buffer* vertexBuffer_{nullptr};
  Buffer* indexBuffer_{nullptr};
  std::vector<SubMesh> subMeshes_;
};

class Model {
  Model(const Model&) = delete;
  Model& operator = (const Model&) = delete;
public:
  Model(Context& context): vertexBuffer_(context), indexBuffer_(context) {}
  Model(Model&& other) noexcept:
    vertexBuffer_(std::move(other.vertexBuffer_)),
    indexBuffer_(std::move(other.indexBuffer_)),
    allMeshes_(std::move(other.allMeshes_)),
    meshMap_(std::move(other.meshMap_)),
    allNodes_(std::move(other.allNodes_)),
    nodeMap_(std::move(other.nodeMap_)),
    topNodes_(std::move(other.topNodes_)) {}
  Model& operator = (Model&& other) noexcept { std::destroy_at(this); std::construct_at(this, std::move(other)); return *this; }
  ~Model() {}

  std::vector<Mesh>& allMeshes() { return allMeshes_; }
  const std::vector<Mesh>& allMeshes() const { return allMeshes_; }

  Mesh* findMesh(const std::string& key) {
    auto it = meshMap_.find(key);
    return (it != meshMap_.end()) ? &allMeshes_[it->second] : nullptr;
  }

  Node* findNode(const std::string& key) {
    auto it = nodeMap_.find(key);
    return (it != nodeMap_.end()) ? it->second : nullptr;
  }

  std::vector<Node*> topNodes() { return topNodes_; }
  const std::vector<Node*> topNodes() const { return topNodes_;}

  void updateTransforms() {
    for (auto& node : topNodes_) {
      node->updateTransform(glm::identity<glm::mat4>());
    }
  }

  void draw(const glm::mat4& topMatrix, DrawContext& drawContext) {
    for (auto& node : topNodes_) {
      node->draw(topMatrix, drawContext);
    }
  }

  void importGltf(AssetCache& assetCache, const std::filesystem::path& filePath);

private:
  Buffer vertexBuffer_;
  Buffer indexBuffer_;

  std::vector<Mesh> allMeshes_;
  std::map<std::string, size_t> meshMap_;

  std::vector<std::unique_ptr<Node>> allNodes_;
  std::map<std::string, Node*> nodeMap_;
  std::vector<Node*> topNodes_;
};

} // namespace <anon>