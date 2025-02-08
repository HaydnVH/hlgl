#include <hlgl/plus/assetcache.h>
#include "shaders/pbr.h"

#include "../core/debug.h"
#include <fmt/format.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace {

template <typename AssetT, typename AssetTrueT = AssetT, typename... ParamTs>
std::shared_ptr<AssetT> constructAndCacheAsset(std::map<std::string, std::weak_ptr<AssetT>>& assetMap, const std::string& name, ParamTs&&... params)
{
  auto [it, inserted] = assetMap.insert({name, {}});
  if (!inserted) return nullptr;

  auto sptr = std::shared_ptr<AssetTrueT>(new AssetTrueT(std::forward<ParamTs>(params)...),
    [&assetMap, it](AssetTrueT* ptr) { delete ptr; assetMap.erase(it); });
  if (!sptr) return nullptr;

  it->second = sptr;
  return sptr;
}

} // namespace <anon>


std::shared_ptr<hlgl::Material> hlgl::AssetCache::loadMaterial(const std::string& name) {
  if (loadedMaterials_.count(name))
    return loadedMaterials_[name].lock();
  else
    return constructAndCacheAsset(loadedMaterials_, name);
}

std::shared_ptr<hlgl::Model> hlgl::AssetCache::loadModel(const std::string& name) {
  if (loadedModels_.count(name))
    return loadedModels_[name].lock();

  auto sptr = constructAndCacheAsset(loadedModels_, name, context_);
  std::filesystem::path filePath(name);
  if (filePath.extension() == ".gltf" || filePath.extension() == ".glb") {
    sptr->importGltf(*this, filePath);
  }
  return sptr;
}

std::shared_ptr<hlgl::Pipeline> hlgl::AssetCache::loadPipeline(const std::string& name) {
  if (loadedPipelines_.count(name))
    return loadedPipelines_[name].lock();
  else
    return nullptr;
}

std::shared_ptr<hlgl::Pipeline> hlgl::AssetCache::loadPipeline(const std::string& name, hlgl::ComputePipelineParams params) {
  if (loadedPipelines_.count(name))
    return loadedPipelines_[name].lock();
  if (!params.sDebugName)
    params.sDebugName = name.c_str();
  auto sptr = constructAndCacheAsset<Pipeline, ComputePipeline>(loadedPipelines_, name, context_, std::move(params));
  return (sptr && sptr->isValid()) ? sptr : nullptr;
}

std::shared_ptr<hlgl::Pipeline> hlgl::AssetCache::loadPipeline(const std::string& name, hlgl::GraphicsPipelineParams params) {
  if (loadedPipelines_.count(name))
    return loadedPipelines_[name].lock();
  if (!params.sDebugName)
    params.sDebugName = name.c_str();
  auto sptr = constructAndCacheAsset<Pipeline, GraphicsPipeline>(loadedPipelines_, name, context_, std::move(params));
  return (sptr && sptr->isValid()) ? sptr : nullptr;
}

std::shared_ptr<hlgl::Shader> hlgl::AssetCache::loadShader(const std::string& name, hlgl::ShaderParams params) {
  if (loadedShaders_.count(name))
    return loadedShaders_[name].lock();
  if (!params.sDebugName)
    params.sDebugName = name.c_str();
  auto sptr = constructAndCacheAsset(loadedShaders_, name, context_, std::move(params));
  return (sptr && sptr->isValid()) ? sptr : nullptr;
}

std::shared_ptr<hlgl::Texture> hlgl::AssetCache::loadTexture(const std::string& name) {
  if (loadedTextures_.count(name))
    return loadedTextures_[name].lock();
  int width {0}, height {0}, numChannels {0};
  std::shared_ptr<hlgl::Texture> sptr {nullptr};
  void* data = stbi_load(name.c_str(), &width, &height, &numChannels, 4);
  if (data) {
    sptr = constructAndCacheAsset(loadedTextures_, name, context_, TextureParams{
      .iWidth = (uint32_t)width,
      .iHeight = (uint32_t)height,
      .eFormat = Format::RGBA8i,
      .eFiltering = FilterMode::Linear,
      .usage = TextureUsage::Sampler,
      .pData = data,
      .sDebugName = name.c_str(),
      });
    stbi_image_free(data);
  }
  return (sptr && sptr->isValid()) ? sptr : nullptr;
}

std::shared_ptr<hlgl::Texture> hlgl::AssetCache::loadTexture(const std::string& name, void* fileData, size_t fileSize) {
  if (loadedTextures_.count(name))
    return loadedTextures_[name].lock();
  int width {0}, height {0}, numChannels {0};
  std::shared_ptr<hlgl::Texture> sptr {nullptr};
  void* data = stbi_load_from_memory(static_cast<stbi_uc*>(fileData), static_cast<int>(fileSize), &width, &height, &numChannels, 4);
  if (data) {
    sptr = constructAndCacheAsset(loadedTextures_, name, context_, TextureParams{
      .iWidth = (uint32_t)width,
      .iHeight = (uint32_t)height,
      .eFormat = Format::RGBA8i,
      .eFiltering = FilterMode::Linear,
      .usage = TextureUsage::Sampler,
      .pData = data,
      .sDebugName = name.c_str(),
      });
    stbi_image_free(data);
  }
  return (sptr && sptr->isValid()) ? sptr : nullptr;
}

std::shared_ptr<hlgl::Texture> hlgl::AssetCache::loadTexture(const std::string& name, hlgl::TextureParams params) {
  if (loadedTextures_.count(name))
    return loadedTextures_[name].lock();
  if (!params.sDebugName)
    params.sDebugName = name.c_str();
  auto sptr = constructAndCacheAsset(loadedTextures_, name, context_, std::move(params));
  return (sptr && sptr->isValid()) ? sptr : nullptr;
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

  // Transparent (blue additively) PBR material pipeline.
  defaultPipelines_.push_back(loadPipeline("hlgl::pipelines/pbr-blendAdditive", hlgl::GraphicsPipelineParams{
    .shaders = {pbr_vert.get(), pbr_frag.get()},
    .depthAttachment = DepthAttachment{.format = Format::D32f},
    .colorAttachments = {ColorAttachment{.format = Format::RGBA8i, .blend = blendAdditive}}
    }));

  // Transparent (blend by alpha) PBR material pipeline.
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

  // hlgl::textures/gray is a pure white texture.
  hlgl::ColorRGBAb grayPixel {127, 127, 127, 255};
  defaultTextures_.push_back(loadTexture("hlgl::textures/gray", hlgl::TextureParams{
    .iWidth = 1, .iHeight = 1,
    .eFormat = hlgl::Format::RGBA8i,
    .usage = hlgl::TextureUsage::Sampler,
    .pData = &grayPixel }));

  // hlgl::textures/black is a pure black texture.
  hlgl::ColorRGBAb blackPixel {0, 0, 0, 255};
  defaultTextures_.push_back(loadTexture("hlgl::textures/black", hlgl::TextureParams{
    .iWidth = 1, .iHeight = 1,
    .eFormat = hlgl::Format::RGBA8i,
    .usage = hlgl::TextureUsage::Sampler,
    .pData = &blackPixel }));

  // hlgl::textures/missing is a 16x16 black/magenta checkerboard.
  hlgl::ColorRGBAb magentaPixel {255, 0, 255, 255};
  std::array<hlgl::ColorRGBAb, 16*16> checkerPixels;
  for (int x {0}; x < 16; ++x) {
    for (int y {0}; y < 16; ++y) {
      checkerPixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? blackPixel : magentaPixel;
    }
  }
  defaultTextures_.push_back(loadTexture("hlgl::textures/missing", hlgl::TextureParams{
    .iWidth = 16, .iHeight = 16,
    .eFormat = hlgl::Format::RGBA8i,
    .usage = hlgl::TextureUsage::Sampler,
    .pData = checkerPixels.data() }));
}