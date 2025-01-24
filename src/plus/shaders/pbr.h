#pragma once

namespace hlgl::glsl {

const char* pbr_vert = R"Shader(
#version 450

layout (set = 0, binding = 0) uniform PerFrame {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
  vec4 ambientColor;
  vec4 sunlightDirection;
  vec4 sunlightColor;
} perFrame;

struct Vertex {
  vec3 position; float u;
  vec3 normal;   float v;
  vec4 color;
};

layout(set = 1, binding = 0) readonly buffer Vertices { Vertex vertices[]; };

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outTexcoord;

layout (push_constant) uniform PushConstants {
  mat4 matrix;
} pushConstants;

void main() {
  Vertex vert = vertices[gl_VertexIndex];

  gl_Position = pushConstants.matrix * vec4(vert.position, 1);
  outNormal = pushConstants.matrix * vec4(vert.normal, 0).xyz;
  outColor = vert.color.rgb;
  outTexCoord = vec2(vert.u, vert.v);
}

)Shader";

const char* pbr_frag = R"Shader(
#version 450

layout (set = 0, binding = 0) uniform PerFrame {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
  vec4 ambientColor;
  vec4 sunlightDirection;
  vec4 sunlightColor;
} perFrame;

layout (set = 1, binding = 1) uniform GltfMaterialData {
  vec4 colorFactor;
  vec4 pbrFactors;
} materialData;

layout (set = 1, binding = 2) uniform sampler2D colorTex;
layout (set = 1, binding = 3) uniform sampler2D normalTex;
layout (set = 1, binding = 4) uniform sampler2D pbrTex;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inTexcoord;

layout (location = 0) out vec4 outColor;

void main() {
  float lightVal = max(dot(inNormal, perFrame.sunlightDirection.xyz), 0.1f);

  vec3 color = inColor * texture(colorTex, inTexcoord).rgb;
  vec3 ambient = color * perFrame.ambientColor.rgb;

  outColor = vec4(color * lightVal * perFrame.sunlightColor.w + ambient, 1.0f);
}

)Shader";

} // namespace hlgl::glsl