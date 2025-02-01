#pragma once
#include <hlgl/hlgl-core.h>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

namespace hlgl {

class Material;
class Mesh;
class Model;

struct DrawEntry {
  hlgl::Buffer* vertexBuffer;
  hlgl::Buffer* indexBuffer;
  uint32_t indexCount;
  uint32_t firstIndex;
  Material* material;
  glm::mat4 transform;
};

struct DrawContext {
  std::vector<DrawEntry> opaqueDraws;
  std::vector<DrawEntry> nonOpaqueDraws;
};

class IDrawable {
  virtual void draw(const glm::mat4&, DrawContext&) = 0;
};

struct Node: public IDrawable {
  Node* parent;
  std::vector<Node*> children;

  glm::mat4 localTransform {glm::identity<glm::mat4>()};
  glm::mat4 worldTransform {glm::identity<glm::mat4>()};

  void updateTransform(const glm::mat4& parentMatrix) {
    worldTransform = parentMatrix * localTransform;
    for (auto& c : children) {
      c->updateTransform(worldTransform);
    }
  }

  virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) {
    for (auto& c : children) {
      c->draw(topMatrix, ctx);
    }
  }
};

struct MeshNode: public Node {
  Mesh* mesh {nullptr};
  virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

} // namespace hlgl
