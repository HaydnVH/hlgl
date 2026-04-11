#pragma once

#include "types.h"

namespace hlgl {

class Context;





class Buffer;
Buffer* createBuffer(BufferParams params);
void destroyBuffer(Buffer* buffer);

using BufferShared = std::shared_ptr<Buffer>;
BufferShared createBufferShared(BufferParams params);

using BufferUnique = std::unique_ptr<Buffer>;
BufferUnique createBufferUnique(BufferParams params);



class Frame;

void updateBufferData(Frame* frame, Buffer* buffer, void* data, size_t size, size_t offset);

} // namespace hlgl