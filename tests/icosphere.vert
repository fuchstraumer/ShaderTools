#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 vPos;
layout(location = 1) out vec3 vNorm;
layout(location = 2) out vec2 vUV;

layout(push_constant) uniform _ubo_data {
    mat4 m;
    mat4 v;
    mat4 p;
} pc;

layout(set = 0, binding = 0) uniform ubo0 {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D heightMap;

layout(set = 1, binding = 1) uniform ubo1 {
    float w0;
    float w1;
    float w2;
    float w3;
} zWarps;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(position, 1.0f) + vec4(zWarps.w0, zWarps.w1, zWarps.w2, zWarps.w3);
    vPos = mat3(ubo.model) * position;
    const mat3 stuff = mat3(pc.p * pc.v * pc.m);
    vNorm = normalize(transpose(inverse(mat3(ubo.model))) * normal);
    vUV = uv;
}