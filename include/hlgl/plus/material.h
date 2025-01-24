#pragma once
#include <hlgl/hlgl-core.h>
#include <span>

namespace hlgl {

enum class MaterialPass: uint8_t {
  Opaque,
  Transparent,
  Other
};

class Material {
public:
  Material(Context& context, GraphicsPipelineParams graphicsPipelineParams, const std::span<std::pair<TextureParams, uint32_t>>& textureParams);

private:
  GraphicsPipeline* pipeline_ {nullptr};
  MaterialPass pass_ {MaterialPass::Other};
  std::vector<std::pair<Texture*, uint32_t>> textures_;
  std::array<glm::vec4, 16> uniformBufferData_;
};

} // namespace hlgl