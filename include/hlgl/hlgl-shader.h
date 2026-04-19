#ifndef HLGL_SHADER_H
#define HLGL_SHADER_H

#include "hlgl-base.h"

namespace hlgl {

struct ShaderImpl;

// The Shader class manages and compiles shaders, allowing them to be used for pipelines.  Once all pipelines using a given shader have been created, the Shader can be safely destroyed.
class Shader {
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;
  public:
  Shader(Shader&&) noexcept = default;
  Shader& operator=(Shader&&) noexcept = default;
  ~Shader();

  struct CreateParams {
    const char* src {nullptr};      // The source code for this shader.  This code will be compiled to Spir-V and then used to create a shader module.
    const void* spvData {nullptr};  // Shaders can also be provided as precompiled Spir-V bytecode.  This is faster than compiling at runtime.
    size_t spvSize {0};             // The size, in bytes, of the provided Spir-V bytecode.
    const char* debugName {nullptr};
    };
  Shader(CreateParams params);

  bool isValid() const { return (bool)_pimpl; }
  operator bool() const { return (bool)_pimpl; }

  std::unique_ptr<ShaderImpl> _pimpl;
};

} // namespace hlgl
#endif // HLGL_SHADER_H