#pragma once

namespace hlgl::glsl {

const char* pbr_vert = R"Shader(
#version 450

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outTexCoord;
layout (location = 2) out vec3 outColor;

struct Vertex {
  vec3 position;
  float u;
  vec3 normal;
  float v;
  vec4 color;
};

layout (binding = 0) readonly buffer Vertices { Vertex vertices[]; };

layout (push_constant) uniform Constants {
  mat4 matrix;
} pushConstants;

void main() {
  Vertex vert = vertices[gl_VertexIndex];

  gl_Position = pushConstants.matrix * vec4(vert.position, 1);
  outNormal = (pushConstants.matrix * vec4(vert.normal, 0)).xyz;
  outColor = vert.color.rgb;
  outTexCoord = vec2(vert.u, vert.v);
}
)Shader";

const char* pbr_frag = R"Shader(
#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in vec3 inColor;

layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform sampler2D myTexture;

void main() {
  outColor = texture(myTexture, inTexCoord);
}
)Shader";

} // namespace hlgl::glsl