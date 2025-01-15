#pragma once

#include <filesystem>
#include <wcgl/core/wcgl-buffer.h>

namespace wcgl {

class Mesh {
public:
  static std::shared_ptr<Mesh> load(const Context& context, const std::filesystem::path& filename);
  ~Mesh() {};

  void draw(Frame& frame);

  struct PushConstants {
    glm::mat4 matrix;
    DeviceAddress address;
  };

private:
  Mesh(const Context& context);
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