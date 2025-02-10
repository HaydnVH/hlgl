#pragma once

namespace hlgl::glsl {

const char* pbr_vert = R"Shader(
#version 450

layout (location = 0) out vec3 outPosWorldspace;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outTexCoord;
layout (location = 3) out vec3 outColor;

struct Vertex {
  vec3 position;
  float u;
  vec3 normal;
  float v;
  vec4 tangent;
  vec4 color;
};

layout (binding = 0) readonly buffer Vertices { Vertex vertices[]; };

struct CameraState {
  mat4 view;        // Matrix that transforms from world space to view space.
  mat4 proj;        // Matrix that transforms from view space to camera space.
  mat4 viewProj;    // Matrix that transforms from world space to camera space.
  mat4 invProj;     // Matrix that transforms from camera space to view space.
  mat4 invViewProj; // Matrix that transforms from camera space to world space.
  vec4 worldPos;    // World space position of the camera in xyz; w holds fov in radians.
};

layout (binding = 2) uniform PerFrame {
  CameraState camera;
} perFrame;

layout (push_constant) uniform Constants {
  mat4 matrix;
  vec4 baseColor;
  vec4 roughnessMetallic;
  vec4 emissive;
} pushConstants;

void main() {
  Vertex vert = vertices[gl_VertexIndex];
  mat4 MVP = perFrame.camera.viewProj * pushConstants.matrix;

  gl_Position = MVP * vec4(vert.position, 1);
  outNormal = (MVP * vec4(vert.normal, 0)).xyz;
  outColor = vert.color.rgb;
  outTexCoord = vec2(vert.u, vert.v);
}
)Shader";

const char* pbr_frag = R"Shader(
#version 450

struct CameraState {
  mat4 view;        // Matrix that transforms from world space to view space.
  mat4 proj;        // Matrix that transforms from view space to camera space.
  mat4 viewProj;    // Matrix that transforms from world space to camera space.
  mat4 invProj;     // Matrix that transforms from camera space to view space.
  mat4 invViewProj; // Matrix that transforms from camera space to world space.
  vec4 worldPos;    // World space position of the camera in xyz; w holds fov in radians.
};

layout (std140, binding = 2) uniform PerFrame {
  CameraState camera;
} perFrame;

layout (push_constant) uniform Constants {
  mat4 matrix;
  vec4 baseColor;
  vec4 roughnessMetallic;
  vec4 emissive;
} pushConstants;

layout (location = 0) in vec3 inPosWorldspace;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec3 inColor;

layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform sampler2D myTexture;

void main() {
  vec3 N = normalize(inNormal);

  outColor = texture(myTexture, inTexCoord) * pushConstants.baseColor;
}
)Shader";

} // namespace hlgl::glsl