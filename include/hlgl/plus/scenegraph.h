#pragma once
#include <hlgl/hlgl-core.h>
#include <glm/glm.hpp>
#include "material.h"

namespace hlgl {

struct RenderObject {
  uint32_t indexCount;
  uint32_t firstIndex;
  hlgl::Buffer vertexBuffer;
  hlgl::Buffer indexBuffer;
  Material* material;
  glm::mat4 transform;
};

}
