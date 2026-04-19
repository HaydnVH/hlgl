#ifndef HLGL_VK_SAMPLER_H
#define HLGL_VK_SAMPLER_H

#include <hlgl.h>
#include "vulkan-includes.h"

namespace hlgl {

class Sampler {
  Sampler(const Sampler&) = delete;
  Sampler& operator=(const Sampler&) = delete;
public:
  Sampler(Sampler&&) noexcept = default;
  Sampler& operator=(Sampler&&) noexcept = default;

  Sampler(CreateSamplerParams&& params);
  ~Sampler();

  bool isValid() const { return sampler; }
  VkSampler getSampler() { return sampler; }

private:
  VkSampler sampler {nullptr};
};

} // namespace hlgl
#endif // HLGL_VK_SAMPLER_H