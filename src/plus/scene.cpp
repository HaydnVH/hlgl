#include <hlgl/plus/scene.h>
#include <hlgl/plus/material.h>
#include <hlgl/plus/model.h>
#include "../core/debug.h"

void hlgl::MeshNode::draw(const glm::mat4& topMatrix, DrawContext& ctx) {
  glm::mat4 matrix = topMatrix * worldTransform;

  if (mesh) {
    for (auto& subMesh : mesh->subMeshes()) {
      DrawEntry entry;
      entry.vertexBuffer = mesh->vertexBuffer();
      entry.indexBuffer = mesh->indexBuffer();
      entry.indexCount = subMesh.count;
      entry.firstIndex = subMesh.start;
      entry.material = subMesh.material.get();
      entry.transform = matrix;

      if (!entry.material || !entry.material->pipeline)
        continue;
      if (entry.material->pipeline->isOpaque())
        ctx.opaqueDraws.push_back(entry);
      else
        ctx.nonOpaqueDraws.push_back(entry);
    }
  }
  Node::draw(topMatrix, ctx);
}