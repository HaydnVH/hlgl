#pragma once
#include <hlgl/hlgl-core.h>

#include <filesystem>
#include <map>
#include <string>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace hlgl {

class Mesh;
using Model = std::map<std::string, hlgl::Mesh>;

class AssetCache;
class Material;

class Mesh {
  Mesh(const Mesh&) = delete;
  Mesh& operator = (const Mesh&) = delete;

public:
  Mesh(Mesh&&) noexcept;
  Mesh& operator = (Mesh&&) noexcept = default;
  Mesh(Context& context): vertexBuffer_(context), indexBuffer_(context) {}
  ~Mesh() {};

  bool isValid() const { return (vertexBuffer_.isValid() && indexBuffer_.isValid() && subMeshes_.size() > 0); }
  operator bool () const { return isValid(); }

  static Model loadGltf(Context& context, const std::filesystem::path& filename);

  const std::string& name() const { return name_; }
  const glm::mat4& matrix() const { return matrix_; }
  Buffer* vertexBuffer() { return &vertexBuffer_; }
  Buffer* indexBuffer() { return &indexBuffer_; }

  struct SubMesh {
    uint32_t start {0};
    uint32_t count {0};
    std::shared_ptr<Material> material {nullptr};
  };

  const std::vector<SubMesh>& subMeshes() const { return subMeshes_; }

private:
  std::string name_ {""};
  glm::mat4 matrix_ {glm::identity<glm::mat4>()};

  Buffer vertexBuffer_;
  Buffer indexBuffer_;

  std::vector<SubMesh> subMeshes_;
};

} // namespace <anon>