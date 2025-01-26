#pragma once

#include <glm/glm.hpp>

namespace hlgl {

struct Vertex {
  glm::vec3 position; float u;
  glm::vec3 normal; float v;
  glm::vec4 color;
};

// Vertex attributes which can be modified by skeletal animation.
struct VertexSkinnableAttributes {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 tangent;
};

// Vertex attributes which are not modified by skeletal animation.
struct VertexSurfaceAttributes {
  glm::vec2 texcoord;
  glm::vec4 color;
};

// Vertex attributes which are used to perform skeletal animation.
struct VertexSkinningAttributes {
  uint8_t boneIndices [4];
  uint8_t boneWeights [4];
};

} // namespace hlgl