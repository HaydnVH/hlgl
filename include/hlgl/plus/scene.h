#pragma once
#include <hlgl/hlgl-core.h>
#include <glm/glm.hpp>
#include "model.h"

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
  std::vector<DrawEntry> translucentDraws;
};

class IDrawable {
  virtual void draw(const glm::mat4&, DrawContext&) = 0;
};

struct Node: public IDrawable {
  std::weak_ptr<Node> parent;
  std::vector<std::shared_ptr<Node>> children;

  glm::mat4 localTransform;
  glm::mat4 worldTransform;

  void refreshTransform(const glm::mat4& parentMatrix) {
    worldTransform = parentMatrix * localTransform;
    for (auto& c : children) {
      c->refreshTransform(worldTransform);
    }
  }

  virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) {
    for (auto& c : children) {
      c->Draw(topMatrix, ctx);
    }
  }
};

struct MeshNode: public Node {
  Mesh* mesh {nullptr};
  virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

} // namespace hlgl
