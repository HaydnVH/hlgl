#pragma once

#include "types.h"

#include <optional>
#include <variant>

namespace hlgl {

class Buffer;
class Context;
class Pipeline;
class Texture;

struct AttachColor {
  Texture* texture {nullptr};
  std::optional<ColorRGBAf> clear {std::nullopt};
};

struct AttachDepthStencil {
  Texture* texture {nullptr};
  std::optional<DepthStencilClearVal> clear {std::nullopt};
};

struct ReadBuffer {
  Buffer* resource {nullptr};
  uint32_t index {UINT32_MAX};
};

struct WriteBuffer {
  Buffer* resource {nullptr};
  uint32_t index {UINT32_MAX};
};

struct ReadTexture {
  Texture* resource {nullptr};
  uint32_t index {UINT32_MAX};
};

struct WriteTexture {
  Texture* resource {nullptr};
  uint32_t index {UINT32_MAX};
};

using Binding = std::variant<ReadBuffer, WriteBuffer, ReadTexture, WriteTexture>;

inline bool isBindBuffer(Binding& binding) { return std::holds_alternative<ReadBuffer>(binding) || std::holds_alternative<WriteBuffer>(binding); }
inline Buffer* getBindBuffer(Binding& binding) {
  if (std::holds_alternative<ReadBuffer>(binding))
    return std::get<ReadBuffer>(binding).resource;
  else if (std::holds_alternative<WriteBuffer>(binding))
    return std::get<WriteBuffer>(binding).resource;
  else
    return nullptr;
}

inline bool isBindTexture(Binding& binding) { return std::holds_alternative<ReadTexture>(binding) || std::holds_alternative<WriteTexture>(binding); }
inline Texture* getBindTexture(Binding& binding) {
  if (std::holds_alternative<ReadTexture>(binding))
    return std::get<ReadTexture>(binding).resource;
  else if (std::holds_alternative<WriteTexture>(binding))
    return std::get<WriteTexture>(binding).resource;
  else
    return nullptr;
}

inline bool isBindRead(Binding& binding) { return std::holds_alternative<ReadBuffer>(binding) || std::holds_alternative<ReadTexture>(binding); }
inline bool isBindWrite(Binding& binding) { return std::holds_alternative<WriteBuffer>(binding) || std::holds_alternative<WriteTexture>(binding); 
}
inline uint32_t getBindIndex(Binding& binding) {
  if (std::holds_alternative<ReadBuffer>(binding))
    return std::get<ReadBuffer>(binding).index;
  else if (std::holds_alternative<WriteBuffer>(binding))
    return std::get<WriteBuffer>(binding).index;
  else if (std::holds_alternative<ReadTexture>(binding))
    return std::get<ReadTexture>(binding).index;
  else if (std::holds_alternative<WriteTexture>(binding))
    return std::get<WriteTexture>(binding).index;
  else
    return UINT32_MAX;
}

class Frame {
  Frame(const Frame&) = delete;
  Frame& operator=(const Frame&) = delete;
public:
  Frame(Frame&&) = default;
  Frame& operator=(Frame&&) = default;

  Frame(Context& context);
  ~Frame();

  bool isValid() const { return initSuccess_; }
  operator bool() const { return initSuccess_; }

  uint32_t getFrameIndex() const;

  struct BlitRegion {
    bool screenRegion {false};
    uint32_t mipLevel {0}, baseLayer {0}, layerCount {1};
    uint32_t x{0}, y{0}, z{0};
    uint32_t w{UINT32_MAX}, h{UINT32_MAX}, d{UINT32_MAX}; };
  void blit(Texture& dst, Texture& src, BlitRegion dstRegion, BlitRegion srcRegion, bool filterLinear = false);

  Texture* getSwapchainTexture();
  std::pair<uint32_t, uint32_t> getViewportSize() const { return {viewportWidth_, viewportHeight_}; }

  void beginDrawing(std::initializer_list<AttachColor> colorAttachments, std::optional<AttachDepthStencil> depthStencilAttachment = std::nullopt);
  void endDrawing();
  void bindPipeline(const Pipeline* pipeline);
  void pushConstants(const void* data, size_t size);
  void pushBindings(std::initializer_list<Binding> bindings, bool barrier);

  void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
  void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
  void drawIndexed(Buffer* indexBuffer, uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, uint32_t vertexOffset = 0, uint32_t firstInstance = 0);

protected:
  Context& context_;
  bool initSuccess_ {false};
  bool inDrawPass_ {false};
  const Pipeline* boundPipeline_ {nullptr};
  const Buffer* boundIndexBuffer_{nullptr};
  uint32_t viewportWidth_ {0}, viewportHeight_ {0};
};

} // namespace hlgl