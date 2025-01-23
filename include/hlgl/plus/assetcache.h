#pragma once

#include <hlgl/core/pipeline.h>
#include <hlgl/core/texture.h>
#include "material.h"
#include "mesh.h"

#include <map>
#include <string>
#include <string_view>
#include <memory>

namespace hlgl {

template <typename T>
using AssetMap = std::map<std::string, std::weak_ptr<T>>;

class AssetCache {
public:
  AssetCache(hlgl::Context& context): context_(context) {}

  std::shared_ptr<Pipeline> loadPipeline(const std::string& name, hlgl::ComputePipelineParams params);
  std::shared_ptr<Pipeline> loadPipeline(const std::string& name, hlgl::GraphicsPipelineParams params);
  std::shared_ptr<Texture>  loadTexture (const std::string& filename);
  std::shared_ptr<Material> loadMaterial(const std::string& filename);
  std::shared_ptr<Mesh>     loadMesh    (const std::string& filename);

  void initDefaultAssets();

private:
  hlgl::Context& context_;
  AssetMap<Pipeline> loadedPipelines_;
  AssetMap<Texture>  loadedTextures_;
  AssetMap<Material> loadedMaterials_;
  AssetMap<Mesh>     loadedMeshess_;

  std::vector<std::shared_ptr<Pipeline>> defaultPipelines_;
  std::vector<std::shared_ptr<Texture>>  defaultTextures_;
  std::vector<std::shared_ptr<Material>> defaultMaterials_;
  std::vector<std::shared_ptr<Mesh>>     defaultMeshes_;
};

}