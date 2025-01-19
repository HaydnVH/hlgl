#pragma once
#include <hlgl/hlgl-core.h>

#include <filesystem>
#include <glm/glm.hpp>

namespace hlgl {

class Frame;

class Mesh {
  Mesh(const Mesh&) = delete;
  Mesh& operator = (const Mesh&) = delete;

public:
  Mesh(Mesh&&) noexcept;
  Mesh& operator = (Mesh&&) noexcept = default;
  Mesh(Context& context): indexBuffer_(context), vertexBuffer_(context) {}
  ~Mesh() {};

  static std::vector<Mesh> loadGltf(Context& context, const std::filesystem::path& filename);

  DeviceAddress getVboDeviceAddress() const { return vertexBuffer_.getDeviceAddress(); }
  int numSubMeshes() const { return (int)subMeshes_.size(); }
  void draw(Frame& frame, int subMeshIndex = -1);

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