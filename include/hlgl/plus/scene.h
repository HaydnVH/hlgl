#pragma once
#include <hlgl/hlgl-core.h>
#include <glm/glm.hpp>
#include "material.h"
#include "mesh.h"

namespace hlgl {

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
  std::vector<DrawEntry> transparentDraws;
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

struct ModelNode: public Node {
  std::shared_ptr<Model> model;
  virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override {
    glm::mat4 matrix = topMatrix * worldTransform;

    for (auto& [name, mesh] : model->meshes()) {
      for (auto& subMesh : mesh.subMeshes()) {
        DrawEntry entry;
        entry.vertexBuffer = &mesh.vertexBuffer();
        entry.indexBuffer = &mesh.indexBuffer();
        entry.indexCount = subMesh.count;
        entry.firstIndex = subMesh.start;
        entry.material = subMesh.material.get();
        entry.transform = matrix;
        ctx.opaqueDraws.push_back(entry);
      }
    }
    Node::Draw(topMatrix, ctx);
  }
};

} // namespace hlgl
