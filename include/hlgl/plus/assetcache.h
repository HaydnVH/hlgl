#pragma once

#include <hlgl/hlgl-core.h>
#include "material.h"
#include "mesh.h"

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <memory>

namespace hlgl {

class AssetCache {
public:
  AssetCache(hlgl::Context& context): context_(context) {}

  std::shared_ptr<Material> loadMaterial(const std::string& name);
  std::shared_ptr<Material> loadMaterial(const std::string& name, void* fileData, size_t fileSize);
  std::shared_ptr<Model>    loadModel   (const std::string& name);
  std::shared_ptr<Model>    loadModel   (const std::string& name, void* fileData, size_t fileSize);
  std::shared_ptr<Pipeline> loadPipeline(const std::string& name);
  std::shared_ptr<Pipeline> loadPipeline(const std::string& name, void* fileData, size_t fileSize);
  std::shared_ptr<Pipeline> loadPipeline(const std::string& name, ComputePipelineParams params);
  std::shared_ptr<Pipeline> loadPipeline(const std::string& name, GraphicsPipelineParams params);
  std::shared_ptr<Shader>   loadShader  (const std::string& name);
  std::shared_ptr<Shader>   loadShader  (const std::string& name, void* fileData, size_t fileSize);
  std::shared_ptr<Shader>   loadShader  (const std::string& name, ShaderParams params);
  std::shared_ptr<Texture>  loadTexture (const std::string& filePath);
  std::shared_ptr<Texture>  loadTexture (const std::string& name, void* fileData, size_t fileSize);
  std::shared_ptr<Texture>  loadTexture (const std::string& name, TextureParams params);

  // Loads a number of default assets and holds on to a reference to them so they don't get freed.
  void initDefaultAssets();

private:
  hlgl::Context& context_;

  std::map<std::string, std::weak_ptr<Material>> loadedMaterials_;
  std::map<std::string, std::weak_ptr<Model>>    loadedModels_;
  std::map<std::string, std::weak_ptr<Pipeline>> loadedPipelines_;
  std::map<std::string, std::weak_ptr<Shader>>   loadedShaders_;
  std::map<std::string, std::weak_ptr<Texture>>  loadedTextures_;

  std::vector<std::shared_ptr<Material>> defaultMaterials_;
  std::vector<std::shared_ptr<Model>>    defaultModels_;
  std::vector<std::shared_ptr<Pipeline>> defaultPipelines_;
  std::vector<std::shared_ptr<Shader>>   defaultShaders_;
  std::vector<std::shared_ptr<Texture>>  defaultTextures_;
};

} // namespace hlgl