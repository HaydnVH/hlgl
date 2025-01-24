#include <hlgl/plus/assetcache.h>
#include "shaders/pbr.h"

std::shared_ptr<hlgl::Pipeline> hlgl::AssetCache::loadPipeline(const std::string& name, hlgl::ComputePipelineParams params) {

  if (loadedPipelines_.count(name))
    return loadedPipelines_ [name].lock();
  else {
    auto [it, inserted] = loadedPipelines_.insert({name, {}});

    auto sptr = std::shared_ptr<ComputePipeline>(new ComputePipeline(context_, std::move(params)),
                                                  [this, it](ComputePipeline* ptr) { delete ptr; loadedPipelines_.erase(it); });
    if (!sptr->isValid())
      return nullptr;
    else {
      it->second = sptr;
      return sptr;
    }
  }
}

std::shared_ptr<hlgl::Pipeline> hlgl::AssetCache::loadPipeline(const std::string& name, hlgl::GraphicsPipelineParams params) {

  if (loadedPipelines_.count(name))
    return loadedPipelines_ [name].lock();
  else {
    auto [it, inserted] = loadedPipelines_.insert({name, {}});
    
    auto sptr = std::shared_ptr<GraphicsPipeline>(new GraphicsPipeline(context_, std::move(params)),
                                                 [this, it](GraphicsPipeline* ptr) { delete ptr; loadedPipelines_.erase(it); });
    if (!sptr->isValid())
      return nullptr;
    else {
      it->second = sptr;
      return sptr;
    }
  }
}

std::shared_ptr<hlgl::Texture> hlgl::AssetCache::loadTexture(const std::string& name, hlgl::TextureParams params) {

  if (loadedTextures_.count(name))
    return loadedTextures_[name].lock();
  else {
    auto [it, inserted] = loadedTextures_.insert({name, {}});

    auto sptr = std::shared_ptr<Texture>(new Texture(context_, std::move(params)),
                                        [this, it](Texture* ptr) { delete ptr; loadedTextures_.erase(it); });
    if (!sptr->isValid())
      return nullptr;
    else {
      it->second = sptr;
      return sptr;
    }
  }
}

void hlgl::AssetCache::initDefaultAssets() {

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

  // Create default pipelines.

}