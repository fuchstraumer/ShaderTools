#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNorm;
layout(location = 3) in vec2 vUV;

layout(push_constant) uniform _fragment_push_data {
    layout(offset = 192) vec4 lightPosition;
    layout(offset = 208) vec4 viewerPosition;
    layout(offset = 224) vec4 lightColor;
} ubo;


layout(set = 0, binding = 0) uniform ubo0 {
    mat4 model;
    mat4 view;
    mat4 projection;
} mvp;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;

layout(set = 1, binding = 2) uniform ubo1 {
    vec4 albedo;
    vec4 diffuse;
    vec4 specular;
    vec4 roughness;
} materialParams;

layout(location = 0) out vec4 fragColor;

void main() {
    
    const float ambient_strength = materialParams.roughness.x;
    vec3 ambient = ambient_strength * ubo.lightColor.xyz;
    const mat4 stuff = mvp.projection * mvp.model * mvp.view;

    const float diffuse_strength = materialParams.diffuse.x;
    vec3 light_direction = normalize(ubo.lightPosition.xyz - vPos);
    float diff = max(dot(vNorm, light_direction), 0.0f);
    vec3 diffuse = diffuse_strength * diff * ubo.lightColor.xyz;
    diffuse *= mat3(stuff);
    float specular_strength = materialParams.specular.x;
    vec3 view_direction = normalize(ubo.viewerPosition.xyz - vPos);
    vec3 reflect_direction = reflect(-light_direction, vNorm);
    float spec = pow(max(dot(view_direction, reflect_direction), 0.0f), 32.0f);
    vec3 specular = specular_strength * spec * ubo.lightColor.xyz;

    vec4 light_result = vec4(ambient + diffuse + specular, 1.0f);
    fragColor = texture(textureSampler, vUV);
    fragColor *= light_result * materialParams.albedo;
    fragColor.a = 1.0f;

}