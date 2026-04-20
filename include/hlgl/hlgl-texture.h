#ifndef HLGL_TEXTURE_H
#define HLGL_TEXTURE_H

#include "hlgl-base.h"

namespace hlgl {

 // How should this image be used?
enum class TextureUsage {
  None        = 0,
  Framebuffer = 1 << 0, // Framebuffer images can be used as attachments for drawing operations.
  ScreenSize  = 1 << 1, // Sets the size of the image to match the display, and resizes it whenever the display resizes.
  Storage     = 1 << 2, // A storage image can be used as arbitrary data storage by shaders.
  TransferSrc = 1 << 3, // Valid source for transfer operations.
  TransferDst = 1 << 4, // Valid destination for transfer operations.
  };
using TextureUsages = Flags<TextureUsage>;
template <> struct FlagsTraits<TextureUsage> { static constexpr bool isFlags {true}; static constexpr int32_t numBits {5}; };

struct TextureImpl;

// Texture represents a block of GPU data which can be rendered to, sampled as a texture, etc.
class Texture {
  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;
  public:
  Texture(Texture&&) noexcept = default;
  Texture& operator=(Texture&&) noexcept = default;
  ~Texture();

  struct CreateParams {
    TextureUsages usage {TextureUsage::None};       // Usage flags
    uint32_t      width {1};                        // Width of the texture, in pixels.  Cannot be 0.
    uint32_t      height {1};                       // Height of the texture, in pixels.  If 0 or 1, the texture will be 1D.
    uint32_t      depth {1};                        // Depth of the texture, in pixels.  If 0 or 1, the texture will be 1D or 2D.
    uint32_t      mipCount {1};                     // Number of mipmap levels in the texture.  Cannot be 0.  Defaults to 1 for no mipmapping.
    uint32_t      mipBase {0};                      // Base mipmap level.  Defaults to 0.
    uint32_t      layerCount {1};                   // Number of layers in the texture.  Defaults to 1 for a non-layered texture.
    uint32_t      layerBase {0};                    // Base layer index.  Defaults to 0.
    ImageFormat   format {ImageFormat::Undefined};  // Pixel format.  Required!
    void*         dataPtr {nullptr};                // Pointer to image data to be copied into the image.
    uint64_t      dataSize {0};                     // Size of the image data to be copied into the image.  If the format isn't compressed, this can be left at 0 and the size will be inferred.
    uint64_t*     mipOffsets {nullptr};             // An array of 'mipCount' offsets into the data pointed to by 'dataPtr' indicating each mip level's region.
    void*         extraData {nullptr};              // Currently unused except for internal purposes.
    const char*   debugName {};

    struct Sampler {
      WrapMode    wrapping {WrapMode::ClampToEdge};   // Wrapping mode to use along any axis which is set to 'Wrapping::DontCare'.  Defaults to ClampToEdge.
      WrapMode    wrapU {WrapMode::DontCare};         // Wrapping mode to use along the X (U) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
      WrapMode    wrapV {WrapMode::DontCare};         // Wrapping mode to use along the Y (V) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
      WrapMode    wrapW {WrapMode::DontCare};         // Wrapping mode to use along the Z (W) axis.  Defaults to 'DontCare' which uses 'eWrapping'.
      FilterMode  filtering {FilterMode::Nearest};    // Filter operation to use for minifaction and magnification.  Defaults to Nearest.
      FilterMode  filterMin {FilterMode::DontCare};   // Filter operation to use for magnification.  Defaults to DontCare.
      FilterMode  filterMag {FilterMode::DontCare};   // Filter operation to use for magnification.  Defaults to DontCare.
      FilterMode  filterMips {FilterMode::DontCare};  // Filter operation to use between mipmaps.  Defaults to Nearest.
      float       maxAnisotropy {8.0f};
      float       maxLod {1.0f};                      // The maximum level of detail, which should be equal to the number of mip levels in the image (in most cases).
      ColorRGBAi  borderColor {255,255,255,255};      // Border color to use if wrap mode is ClampToBorder.
      };
    std::optional<Sampler> sampler {std::nullopt};
    };
  Texture(CreateParams params);

  bool isValid() const { return (bool)_pimpl; }
  operator bool() const { return (bool)_pimpl; }

  void getDimensions(uint32_t& w, uint32_t& h, uint32_t& d) const;
  ImageFormat getFormat() const;

  void barrier(ImageLayout layout, bool read);
  void readBarrier(ImageLayout layout) { barrier(layout, true); }
  void writeBarrier(ImageLayout layout) { barrier(layout, false); }

  uint32_t getDescIndexImageSampler() const;
  uint32_t getDescIndexStorageImage() const;

  std::unique_ptr<TextureImpl> _pimpl;
};

} // namespace hlgl
#endif // HLGL_TEXTURE_H