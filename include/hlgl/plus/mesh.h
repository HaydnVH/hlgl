#pragma once

#include <filesystem>
#include <hlgl/core/buffer.h>
#include <glm/glm.hpp>

namespace hlgl {

class Frame;

class Mesh {
  Mesh(const Mesh&) = delete;
  Mesh& operator = (const Mesh&) = delete;

public:
  Mesh(Mesh&&);
  Mesh& operator = (Mesh&&) noexcept = default;
  Mesh(const Context& context): indexBuffer_(context), vertexBuffer_(context) {}
  ~Mesh() {};

  static std::vector<Mesh> loadGltf(const Context& context, const std::filesystem::path& filename);

  DeviceAddress getVboDeviceAddress() { return vertexBuffer_.getDeviceAddress(); }
  void draw(Frame& frame);

  struct PushConstants {
    glm::mat4 matrix;
    DeviceAddress address;
  };

private:
  std::string name_;

  Buffer indexBuffer_;
  Buffer vertexBuffer_;

  struct SubMesh {
    uint32_t start;
    uint32_t count;
  };
  std::vector<SubMesh> subMeshes_;
};

}