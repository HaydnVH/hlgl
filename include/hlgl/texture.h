#pragma once

#include "types.h"

namespace hlgl {

class Context;

// How should this texture be used?
enum class TextureUsage : uint16_t {
  DontCare    = 0,
  Framebuffer = 1 << 0, // The texture will be drawn to and potentially copied to the display at the end of the frame.
  Sampler     = 1 << 1, // The texture will be sampled in a shader using regular texture coordinates.
  ScreenSize  = 1 << 2, // The texture will be the same size as the display and will be resized along with the window.
  Storage     = 1 << 3, // The texture will be treated as generic data storage.
  TransferSrc = 1 << 4, // The texture will be used as a source for transfer operations.
  TransferDst = 1 << 5, // The texture will be used as a destination for transfer operations.
};
using TextureUsages = Flags<TextureUsage>;
template <> struct FlagsTraits<TextureUsage> {
  static constexpr bool isFlags {true};
  static constexpr int32_t numBits {6};
};

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

class Texture {
  friend class Context;
  friend class Frame;

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

  Format format() const;
  uint32_t getWidth() const;
  uint32_t getHeight() const;
  uint32_t getDepth() const;

  bool resize();
  bool resize(uint32_t newWidth, uint32_t newHeight, uint32_t newDepth);

  #if defined HLGL_GRAPHICS_API_VULKAN
  VkImage getVkImage() { return image_; }
  VkImageView getVkImageView() { return view_; }
  VkSampler getVkSampler() { return sampler_; }
  #endif

private:
  bool initSuccess_ {false};
  TextureParams savedParams_;

#if defined HLGL_GRAPHICS_API_VULKAN

  VkImage image_{nullptr};
  VmaAllocation allocation_{nullptr};
  VmaAllocationInfo allocInfo_{};
  VkImageView view_{nullptr};
  VkSampler sampler_{nullptr};
  VkExtent3D extent_{1,1,1};
  uint32_t mipIndex_{0};
  uint32_t mipCount_{1};
  VkFormat format_{VK_FORMAT_UNDEFINED};

  VkImageLayout layout_{VK_IMAGE_LAYOUT_UNDEFINED};
  VkAccessFlags accessMask_{0};
  VkPipelineStageFlags stageMask_{0};

  void barrier(
    VkCommandBuffer cmd,
    VkImageLayout dstLayout,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags dstStageMask);

#endif // defined HLGL_GRAPHICS_API_x
};

} // namespace hlgl