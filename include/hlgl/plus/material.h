#pragma once
#include <hlgl/hlgl-core.h>
#include <span>

#include <glm/glm.hpp>

namespace hlgl {

struct MaterialTextureBinding {
  std::shared_ptr<Texture> texture;
  uint32_t binding;
};

class Material {
public:
  Material() = default;

  bool isValid() const { return true; }

  std::shared_ptr<Pipeline> pipeline {nullptr};
  std::vector<MaterialTextureBinding> textures {};
  std::array<glm::vec4, 16> uniformBufferData {};
};

} // namespace hlgl