#pragma once

#include "hlgl-types.h"

namespace hlgl {

class Context;



struct TextureParams {
  uint32_t width {1};            // Width of the texture, in pixels.  Cannot be 0.
  uint32_t height {1};           // Height of the texture, in pixels.  If 0 or 1, the texture will be 1D.
  uint32_t depth {1};            // Depth of the texture, in pixels.  If 0 or 1, the texture will be 1D or 2D.
  uint32_t mipCount {1};         // Number of mipmap levels in the texture.  Cannot be 0.  Defaults to 1 for no mipmapping.
  uint32_t mipBase {0};          // Base mipmap level.  Defaults to 0.
  uint32_t layerCount {1};       // Number of layers in the texture.  Defaults to 1 for a non-layered texture.
  uint32_t layerBase {0};        // Base layer index.  Defaults to 0.
  Format format {Format::Undefined};         // Pixel format.  Required.
  WrapMode wrapping {WrapMode::ClampToEdge}; // Wrapping mode to use along any axis which is set to 'Wrapping::DontCare'.  Defaults to ClampToEdge.
  WrapMode wrapU {WrapMode::DontCare};       // Wrapping mode to use along the X (U) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
  WrapMode wrapV {WrapMode::DontCare};       // Wrapping mode to use along the Y (V) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
  WrapMode wrapW {WrapMode::DontCare};       // Wrapping mode to use along the Z (W) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
  FilterMode filtering {FilterMode::Nearest};   // Filter operation to use for minifaction and magnification.  Defaults to Nearest.
  FilterMode filterMin {FilterMode::DontCare};  // Filter operation to use for magnification.  Defaults to DontCare.
  FilterMode filterMag {FilterMode::DontCare};  // Filter operation to use for magnification.  Defaults to DontCare.
  FilterMode filterMips {FilterMode::DontCare};  // Filter operation to use between mipmaps.  Defaults to Nearest.
  float maxAnisotropy {8.0f};
  float maxLod {16.0f};
  ColorRGBAi borderColor {255,255,255,255};
  TextureUsages usage {TextureUsage::DontCare}; // Usage flags.
  const void* dataPtr {nullptr};
  std::string debugName {};
  void* _existingImage{nullptr}; // Used to create a texture from an existing image.  Internal use only!
};

class Texture;
Texture* createTexture(TextureParams params);
void destroyTexture(Texture* texture);

using TextureShared = std::shared_ptr<Texture>;
TextureShared createTextureShared(TextureParams params);

using TextureUnique = std::unique_ptr<Texture>;
TextureUnique createTextureUnique(TextureParams params);


/*
class Texture {
  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

public:
  Texture(Texture&& other) noexcept = default;
  Texture& operator=(Texture&&) noexcept = default;

  Texture() {}
  Texture(TextureParams&& params) { Construct(params); }
  void Construct(TextureParams params);
  ~Texture();

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

  Format getFormat() const;
  uint32_t getWidth() const;
  uint32_t getHeight() const;
  uint32_t getDepth() const;

  bool resize();
  bool resize(uint32_t newWidth, uint32_t newHeight, uint32_t newDepth);

  #if defined HLGL_GRAPHICS_API_VULKAN
  struct VK {
    VkImage image{nullptr};
    VmaAllocation allocation{nullptr};
    VmaAllocationInfo allocInfo{};
    VkImageView view{nullptr};
    VkSampler sampler{nullptr};
    VkExtent3D extent{1,1,1};
    uint32_t mipIndex{0};
    uint32_t mipCount{1};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkAccessFlags accessMask{0};
    VkPipelineStageFlags stageMask{0};
    
    void barrier(
      VkCommandBuffer cmd,
      VkImageLayout dstLayout,
      VkAccessFlags dstAccessMask,
      VkPipelineStageFlags dstStageMask);
  } _vk;

  #endif

private:
  bool initSuccess_ {false};
  TextureParams savedParams_;
};

*/

} // namespace hlgl