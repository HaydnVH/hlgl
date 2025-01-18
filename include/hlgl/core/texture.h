#pragma once

#include "types.h"

namespace hlgl {

class Context;

// How should this texture be used?
enum class TextureUsage : uint16_t {
  DontCare    = 0,
  Framebuffer = 1 << 0, // The texture will be drawn to and potentially copied to the display at the end of the frame.
  Sampler     = 1 << 1, // The texture will be sampled in a shader using regular texture coordinates.
  Storage     = 1 << 2, // The texture will be treated as generic data storage.
  TransferSrc = 1 << 3, // The texture will be used as a source for transfer operations.
  TransferDst = 1 << 4, // The texture will be used as a destination for transfer operations.
};
template <> struct isBitfield<TextureUsage> : public std::true_type {};
using TextureUsages = Flags<TextureUsage>;

struct TextureParams {
  bool bMatchDisplaySize {false}; // If true, iWidth iHeight and iDepth are ignored, and a 2D texture with the size of the current display is created.
  uint32_t iWidth {1};            // Width of the texture, in pixels.  Cannot be 0.
  uint32_t iHeight {1};           // Height of the texture, in pixels.  If 0 or 1, the texture will be 1D.
  uint32_t iDepth {1};            // Depth of the texture, in pixels.  If 0 or 1, the texture will be 1D or 2D.
  uint32_t iMipCount {1};         // Number of mipmap levels in the texture.  Cannot be 0.  Defaults to 1 for no mipmapping.
  uint32_t iMipBase {0};          // Base mipmap level.  Defaults to 0.
  uint32_t iLayerCount {1};       // Number of layers in the texture.  Defaults to 1 for a non-layered texture.
  uint32_t iLayerBase {0};        // Base layer index.  Defaults to 0.
  Format eFormat {Format::Undefined};         // Pixel format.  Required.
  WrapMode eWrapping {WrapMode::ClampToEdge}; // Wrapping mode to use along any axis which is set to 'Wrapping::DontCare'.  Defaults to ClampToEdge.
  WrapMode eWrapU {WrapMode::DontCare};       // Wrapping mode to use along the X (U) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
  WrapMode eWrapV {WrapMode::DontCare};       // Wrapping mode to use along the Y (V) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
  WrapMode eWrapW {WrapMode::DontCare};       // Wrapping mode to use along the Z (W) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
  FilterMode eFiltering {FilterMode::Nearest};   // Filter operation to use for minifaction and magnification.  Defaults to Nearest.
  FilterMode eFilterMin {FilterMode::DontCare};  // Filter operation to use for magnification.  Defaults to DontCare.
  FilterMode eFilterMag {FilterMode::DontCare};  // Filter operation to use for magnification.  Defaults to DontCare.
  FilterMode eFilterMips {FilterMode::DontCare};  // Filter operation to use between mipmaps.  Defaults to Nearest.
  float fMaxAnisotropy {8.0f};
  float fMaxLod {16.0f};
  ColorRGBAi ivBorderColor {255,255,255,255};
  TextureUsages eUsage {TextureUsage::DontCare}; // Usage flags.
  const void* pData {nullptr};
  const char* sDebugName {nullptr};
  const VkImage pExistingImage{nullptr}; // Used to create a texture from an existing VkImage, most commonly from the swapchain.
};

class Texture {
  friend class Frame;

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

public:
  Texture(Texture&& other) noexcept;
  Texture& operator=(Texture&&) = default;

  Texture(const Context& context): context_(context) {}
  Texture(const Context& context, TextureParams&& params): context_(context) { Construct(params); }
  void Construct(TextureParams params);
  ~Texture();

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

  Format format() const;

private:
  const Context& context_;
  bool initSuccess_ {false};
  std::string debugName_ {};

#if defined HLGL_GRAPHICS_API_VULKAN

  VkImage image_{nullptr};
  VmaAllocation allocation_{nullptr};
  VkImageView view_{nullptr};
  VkSampler sampler_{nullptr};
  VkExtent3D extent_{1,1,1};
  uint32_t mipIndex_{0};
  uint32_t mipCount_{1};
  VkFormat format_{};

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