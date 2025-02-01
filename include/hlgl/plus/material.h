#pragma once
#include <hlgl/hlgl-core.h>
#include <span>

#include <glm/glm.hpp>

namespace hlgl {

class Material {
public:
  Material() = default;

  bool isValid() const { return true; }

  std::shared_ptr<Pipeline> pipeline {nullptr};

  struct Textures {
    std::shared_ptr<Texture> baseColor {nullptr};
    std::shared_ptr<Texture> normal {nullptr};
    std::shared_ptr<Texture> occlusionRoughnessMetallic {nullptr};
    std::shared_ptr<Texture> emissive {nullptr};
  } textures;

  struct Uniforms {
    glm::vec4 baseColor {1,1,1,1};
    glm::vec2 roughnessMetallic {1,1}; glm::vec2 _padding0 {};
    glm::vec4 emissive {0,0,0,1};
  } uniforms;

};

} // namespace hlgl