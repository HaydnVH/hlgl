#pragma once
#include <hlgl/hlgl-core.h>
#include <span>

namespace hlgl {

class Material {
public:
  Material(Context& context, GraphicsPipelineParams graphicsPipelineParams, const std::span<std::pair<TextureParams, uint32_t>>& textureParams);

private:
  GraphicsPipeline* pipeline_;
  std::vector<std::pair<Texture, uint32_t>> textures_;
};

}