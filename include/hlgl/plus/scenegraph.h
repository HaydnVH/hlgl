#pragma once
#include <hlgl/hlgl-core.h>
#include <glm/glm.hpp>
#include "material.h"

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
  virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

} // namespace hlgl
