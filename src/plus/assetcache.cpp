#include <hlgl/plus/assetcache.h>

std::shared_ptr<hlgl::Pipeline> hlgl::AssetCache::loadPipeline(const std::string& name, hlgl::ComputePipelineParams params) {

  if (loadedPipelines_.count(name)) {
    return loadedPipelines_ [name].lock();
  } else {
    auto [it, inserted] = loadedPipelines_.insert({name, {}});

    auto sptr = std::shared_ptr<ComputePipeline>(new ComputePipeline(context_, params),
                                                  [this, it](ComputePipeline* ptr) { delete ptr; loadedPipelines_.erase(it); });
    it->second = sptr;
    return sptr;
  }
}

std::shared_ptr<hlgl::Pipeline> hlgl::AssetCache::loadPipeline(const std::string& name, hlgl::GraphicsPipelineParams params) {

  if (loadedPipelines_.count(name)) {
    return loadedPipelines_ [name].lock();
  } else {
    auto [it, inserted] = loadedPipelines_.insert({name, {}});
    
    auto sptr = std::shared_ptr<GraphicsPipeline>(new GraphicsPipeline(context_, params),
                                                 [this, it](GraphicsPipeline* ptr) { delete ptr; loadedPipelines_.erase(it); });
    it->second = sptr;
    return sptr;
  }
}

void hlgl::AssetCache::initDefaultAssets() {

}