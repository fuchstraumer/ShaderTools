#version 450
layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

struct AABB
{
    vec4 Min;
    vec4 Max;
};

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

layout(set = 0, binding = 4, std430) buffer global_aabb
{
    AABB Data[];
} LightAABBs;

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

layout(set = 1, binding = 1, std430) buffer spot_lights
{
    SpotLight Data[];
} SpotLights;

layout(set = 0, binding = 0, r32ui) uniform writeonly uimageBuffer PointLightMortonCodes;
layout(set = 0, binding = 1, r32ui) uniform writeonly uimageBuffer PointLightIndices;
layout(set = 0, binding = 2, r32ui) uniform writeonly uimageBuffer SpotLightMortonCodes;
layout(set = 0, binding = 3, r32ui) uniform writeonly uimageBuffer SpotLightIndices;

shared AABB gs_AABB;
shared vec4 gs_AABBRange;

void main()
{
    if (gl_LocalInvocationIndex == 0u)
    {
        gs_AABB.Min = LightAABBs.Data[0].Min;
        gs_AABB.Max = LightAABBs.Data[0].Max;
        gs_AABBRange = vec4(1.0) / (gs_AABB.Max - gs_AABB.Min);
    }
    groupMemoryBarrier();
    if (gl_GlobalInvocationID.x < LightCounts.NumPointLights)
    {
        vec4 _133 = PointLights.Data[gl_GlobalInvocationID.x].PositionViewSpace;
        vec4 _135 = gs_AABB.Min;
        vec4 _137 = gs_AABBRange;
        uvec4 _141 = uvec4(((_133 - _135) * _137) * 1023.0);
        int _147 = int(gl_GlobalInvocationID.x);
        uint _324;
        _324 = 0u;
        for (uint _323 = 1u, _328 = 0u; _323 < uint(1024); )
        {
            uint _236 = _328 + 0u;
            uint _245 = _328 + 1u;
            uint _254 = _328 + 2u;
            _328 = _254;
            _324 = ((_324 | ((_141.x & _323) << _236)) | ((_141.y & _323) << _245)) | ((_141.z & _323) << _254);
            _323 = _323 << uint(1);
            continue;
        }
        imageStore(PointLightMortonCodes, _147, uvec4(_324, 0u, 0u, 0u));
        imageStore(PointLightIndices, _147, uvec4(gl_GlobalInvocationID.x, 0u, 0u, 0u));
    }
    if (gl_GlobalInvocationID.x < LightCounts.NumSpotLights)
    {
        vec4 _175 = SpotLights.Data[gl_GlobalInvocationID.x].PositionViewSpace;
        vec4 _177 = gs_AABB.Min;
        vec4 _179 = gs_AABBRange;
        uvec4 _182 = uvec4(((_175 - _177) * _179) * 1023.0);
        int _186 = int(gl_GlobalInvocationID.x);
        uint _326;
        _326 = 0u;
        for (uint _325 = 1u, _327 = 0u; _325 < uint(1024); )
        {
            uint _284 = _327 + 0u;
            uint _293 = _327 + 1u;
            uint _302 = _327 + 2u;
            _327 = _302;
            _326 = ((_326 | ((_182.x & _325) << _284)) | ((_182.y & _325) << _293)) | ((_182.z & _325) << _302);
            _325 = _325 << uint(1);
            continue;
        }
        imageStore(SpotLightMortonCodes, _186, uvec4(_326, 0u, 0u, 0u));
        imageStore(SpotLightIndices, _186, uvec4(gl_GlobalInvocationID.x, 0u, 0u, 0u));
    }
}

