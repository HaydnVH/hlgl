#include <hlgl/plus/assetcache.h>
#include "shaders/pbr.h"

namespace {

template <typename AssetT, typename ParamsT, typename AssetTrueT = AssetT>
std::shared_ptr<AssetT> loadAssetFromParams(std::map<std::string, std::weak_ptr<AssetT>>& assetMap, const std::string& name, ParamsT&& params) {

  if (assetMap.count(name))
    return assetMap [name].lock();
  else {
    if (!params.sDebugName)
      params.sDebugName = name.c_str();
    auto [i, inserted] = assetMap.insert({name, {}});
    auto sptr = std::shared_ptr<AssetTrueT>(new AssetTrueT(context_, std::move(params)),
                                        [assetMap, it](AssetTrueT* ptr) { delete ptr; assetMap.erase(it); });
    if (!sptr || !sptr->isValid())
      return nullptr;
    else {
      it->second = sptr;
      return sptr;
    }
  }
}

} // namespace <anon>

std::shared_ptr<hlgl::Pipeline> hlgl::AssetCache::loadPipeline(const std::string& name, hlgl::ComputePipelineParams params) {
  return loadAssetFromParams<Pipeline, ComputePipelineParams, ComputePipeline>(loadedPipelines_, name, std::move(params));
}

std::shared_ptr<hlgl::Pipeline> hlgl::AssetCache::loadPipeline(const std::string& name, hlgl::GraphicsPipelineParams params) {
  return loadAssetFromParams<Pipeline, GraphicsPipelineParams, GraphicsPipeline>(loadedPipelines_, name, std::move(params));
}

std::shared_ptr<hlgl::Shader> hlgl::AssetCache::loadShader(const std::string& name, hlgl::ShaderParams params) {
  return loadAssetFromParams(loadedShaders_, name, std::move(params));
}

std::shared_ptr<hlgl::Texture> hlgl::AssetCache::loadTexture(const std::string& name, hlgl::TextureParams params) {
  return loadAssetFromParams(loadedTextures_, name, std::move(params));
}

void hlgl::AssetCache::initDefaultAssets() {

  // Create default shaders.

  // Vertex shader for the built-in PBR material.
  auto pbr_vert = loadShader("hlgl::shaders/pbr.vert", hlgl::ShaderParams{.sGlsl = glsl::pbr_vert});
  defaultShaders_.push_back(pbr_vert);

  // Fragment shader for the built-in PBR material.
  auto pbr_frag = loadShader("hlgl::shaders/pbr.frag", hlgl::ShaderParams{.sGlsl = glsl::pbr_frag});
  defaultShaders_.push_back(pbr_frag);

  // Create default pipelines.

  // Opaque PBR material pipeline.
  defaultPipelines_.push_back(loadPipeline("hlgl::pipelines/pbr-opaque", hlgl::GraphicsPipelineParams{
    .shaders = {pbr_vert.get(), pbr_frag.get()},
    .depthAttachment = DepthAttachment{.format = Format::D32f},
    .colorAttachments = {ColorAttachment{.format = Format::RGBA8i}}
  }));

  // Transparent (alpha blended) PBR material pipeline.
  defaultPipelines_.push_back(loadPipeline("hlgl::pipelines/pbr-blendAlpha", hlgl::GraphicsPipelineParams{
    .shaders = {pbr_vert.get(), pbr_frag.get()},
    .depthAttachment = DepthAttachment{.format = Format::D32f},
    .colorAttachments = {ColorAttachment{.format = Format::RGBA8i, .blend = blendAlpha}}
  }));

  // Create default textures.

  // hlgl::textures/white is a pure white texture.
  hlgl::ColorRGBAb whitePixel {255, 255, 255, 255};
  defaultTextures_.push_back(loadTexture("hlgl::textures/white", hlgl::TextureParams{
    .iWidth = 1, .iHeight = 1,
    .eFormat = hlgl::Format::RGBA8i,
    .usage = hlgl::TextureUsage::Sampler,
    .pData = &whitePixel }));

  // hlgl::textures/black is a pure black texture.
  hlgl::ColorRGBAb blackPixel {0, 0, 0, 255};
  defaultTextures_.push_back(loadTexture("hlgl::textures/black", hlgl::TextureParams{
    .iWidth = 1, .iHeight = 1,
    .eFormat = hlgl::Format::RGBA8i,
    .usage = hlgl::TextureUsage::Sampler,
    .pData = &blackPixel }));

  // hlgl::textures/missing is a 16x16 cyan/magenta checkerboard.
  hlgl::ColorRGBAb cyanPixel {0, 255, 255, 255};
  hlgl::ColorRGBAb magentaPixel {255, 0, 255, 255};
  std::array<hlgl::ColorRGBAb, 16*16> checkerPixels;
  for (int x {0}; x < 16; ++x) {
    for (int y {0}; y < 16; ++y) {
      checkerPixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? cyanPixel : magentaPixel;
    }
  }
  defaultTextures_.push_back(loadTexture("hlgl::textures/missing", hlgl::TextureParams{
    .iWidth = 16, .iHeight = 16,
    .eFormat = hlgl::Format::RGBA8i,
    .usage = hlgl::TextureUsage::Sampler,
    .pData = checkerPixels.data() }));

}