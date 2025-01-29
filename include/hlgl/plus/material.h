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

  std::shared_ptr<Texture> texBaseColor {nullptr};
  std::shared_ptr<Texture> texNormal {nullptr};
  std::shared_ptr<Texture> texORM {nullptr};
  std::shared_ptr<Texture> texEmissive {nullptr};

  std::vector<MaterialTextureBinding> otherTextures {};
  std::array<glm::vec4, 16> uniformBufferData {};
};

} // namespace hlgl