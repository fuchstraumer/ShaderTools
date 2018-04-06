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

layout(set = 0, binding = 4, std140) uniform mvp_matrix_ubo
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

void main()
{
    if (gl_GlobalInvocationID.x < LightCounts.NumPointLights)
    {
        PointLights.Data[gl_GlobalInvocationID.x].Position = matrices.Model * PointLights.Data[gl_GlobalInvocationID.x].Position;
        PointLights.Data[gl_GlobalInvocationID.x].PositionViewSpace = matrices.View * vec4(PointLights.Data[gl_GlobalInvocationID.x].Position.xyz, 1.0);
    }
    if (gl_GlobalInvocationID.x < LightCounts.NumSpotLights)
    {
        SpotLights.Data[gl_GlobalInvocationID.x].Position = matrices.Model * SpotLights.Data[gl_GlobalInvocationID.x].Position;
        SpotLights.Data[gl_GlobalInvocationID.x].PositionViewSpace = matrices.View * vec4(SpotLights.Data[gl_GlobalInvocationID.x].Position.xyz, 1.0);
        SpotLights.Data[gl_GlobalInvocationID.x].Direction = matrices.Model * SpotLights.Data[gl_GlobalInvocationID.x].Direction;
        SpotLights.Data[gl_GlobalInvocationID.x].DirectionViewSpace = normalize(matrices.View * vec4(SpotLights.Data[gl_GlobalInvocationID.x].Direction.xyz, 0.0));
    }
    if (gl_GlobalInvocationID.x < LightCounts.NumDirectionalLights)
    {
        DirectionalLights.Data[gl_GlobalInvocationID.x].Direction = matrices.Model * DirectionalLights.Data[gl_GlobalInvocationID.x].Direction;
        DirectionalLights.Data[gl_GlobalInvocationID.x].DirectionViewSpace = normalize(matrices.View * vec4(DirectionalLights.Data[gl_GlobalInvocationID.x].Direction.xyz, 0.0));
    }
}

