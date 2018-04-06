#version 450
layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

struct PointLight
{
    vec4 Position;
    vec4 PositionViewSpace;
    vec3 Color;
    float Range;
    float Intensity;
    uint Enabled;
    vec2 Padding;
};

struct SpotLight
{
    vec4 Position;
    vec4 PositionViewSpace;
    vec4 Direction;
    vec4 DirectionViewSpace;
    vec3 Color;
    float SpotLightAngle;
    float Range;
    float Intensity;
    uint Enabled;
    float Padding;
};

struct DirectionalLight
{
    vec4 Direction;
    vec4 DirectionViewSpace;
    vec3 Color;
    float Intensity;
    uint Enabled;
    vec3 Padding;
};

layout(set = 1, binding = 3, std140) uniform light_counts
{
    uint NumPointLights;
    uint NumSpotLights;
    uint NumDirectionalLights;
} LightCounts;

layout(set = 1, binding = 0, std430) buffer point_lights
{
    PointLight Data[];
} PointLights;

layout(binding = 4, std140) uniform mvp_matrix_ubo
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
    mat4 Normal;
} matrices;

layout(set = 1, binding = 1, std430) buffer spot_lights
{
    SpotLight Data[];
} SpotLights;

layout(set = 1, binding = 2, std430) buffer directional_lights
{
    DirectionalLight Data[];
} DirectionalLights;

layout(binding = 5, std140) uniform misc_data_ubo
{
    vec4 viewPosition;
    vec2 mousePosition;
    vec2 windowSize;
    vec2 depthRange;
    uint frame;
} globals;

layout(binding = 6, std140) uniform lighting_data_ubo
{
    float Exposure;
    float Gamma;
    uint NumLights;
} lighting_data;

layout(binding = 0) uniform sampler2D LinearRepeatSampler;
layout(binding = 1) uniform sampler2D LinearClampSampler;
layout(binding = 2) uniform sampler2D AnisotropicRepeatSampler;
layout(binding = 3) uniform sampler2D AnisotropicClampSampler;

void main()
{
    uint curr_idx = gl_GlobalInvocationID.x;
    if (curr_idx < LightCounts.NumPointLights)
    {
        PointLights.Data[curr_idx].Position = matrices.Model * PointLights.Data[curr_idx].Position;
        PointLights.Data[curr_idx].PositionViewSpace = matrices.View * vec4(PointLights.Data[curr_idx].Position.xyz, 1.0);
    }
    if (curr_idx < LightCounts.NumSpotLights)
    {
        SpotLights.Data[curr_idx].Position = matrices.Model * SpotLights.Data[curr_idx].Position;
        SpotLights.Data[curr_idx].PositionViewSpace = matrices.View * vec4(SpotLights.Data[curr_idx].Position.xyz, 1.0);
        SpotLights.Data[curr_idx].Direction = matrices.Model * SpotLights.Data[curr_idx].Direction;
        SpotLights.Data[curr_idx].DirectionViewSpace = normalize(matrices.View * vec4(SpotLights.Data[curr_idx].Direction.xyz, 0.0));
    }
    if (curr_idx < LightCounts.NumDirectionalLights)
    {
        DirectionalLights.Data[curr_idx].Direction = matrices.Model * DirectionalLights.Data[curr_idx].Direction;
        DirectionalLights.Data[curr_idx].DirectionViewSpace = normalize(matrices.View * vec4(DirectionalLights.Data[curr_idx].Direction.xyz, 0.0));
    }
}

