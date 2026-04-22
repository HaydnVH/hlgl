#ifndef HLGL_BUFFER_H
#define HLGL_BUFFER_H

#include "hlgl-base.h"

namespace hlgl {

// How should this buffer be used?
enum class BufferUsage {
  None              = 0,
  DeviceAddressable = 1 << 1, // The buffer's device address can be retrieved and used.
  HostVisible       = 1 << 2, // A host-visible buffer can have its contents be written to by memcpy.
  Index             = 1 << 3, // Index buffers store indices for indexed drawing.
  Indirect          = 1 << 4, // Indirect buffers can hold indirect draw/dispatch parameters.
  Storage           = 1 << 5, // Storage buffers are for arbitrary data storage.
  TransferSrc       = 1 << 6, // Valid source for transfer operations.
  TransferDst       = 1 << 7, // Valid destination for transfer operations.
  Uniform           = 1 << 8, // Uniform buffers are shared across draws and are typically updated every frame by the CPU.
  Updateable        = 1 << 9, // This buffer can be updated each frame.
  Vertex            = 1 << 10, // The buffer will contain vertices (not neccessary if using buffer device address).
  };
using BufferUsages = Flags<BufferUsage>;
template <> struct FlagsTraits<BufferUsage> { static constexpr bool isFlags {true}; static constexpr int32_t numBits {11}; };

struct BufferImpl;

// Buffer represents a block of arbitrary GPU data.
class Buffer {
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;
  public:
  Buffer(Buffer&&) noexcept = default;
  Buffer& operator=(Buffer&&) noexcept = default;
  ~Buffer();

  struct CreateParams {
    BufferUsages  usage {BufferUsage::None};  // Usage flags
    struct Data { const void* ptr {nullptr}; size_t size {0}; };
    std::initializer_list<Data> data;         // Blocks of data to be copied into the new buffer and their sizes.
    DeviceSize    size {0};                   // Size of the buffer.  If 'data' is non-empty, this is added to the total size.
    uint32_t      indexSize {4};              // The number of bytes in each element of the index buffer.
    const char*   debugName {nullptr};        // Name used for debug messages.
    };
  Buffer(CreateParams params);

  bool isValid() const { return (bool)_pimpl; }
  operator bool() const { return (bool)_pimpl; }

  DeviceAddress getAddress(Frame* frame) const;
  DeviceSize getSize() const;

  void updateData(void* data, size_t size, DeviceSize offset);

  void barrier(bool read);
  void readBarrier() { barrier(true); }
  void writeBarrier() { barrier(false); }

  std::unique_ptr<BufferImpl> _pimpl;
};



} // namespace hlgl
#endif // HLGL_BUFFER_H