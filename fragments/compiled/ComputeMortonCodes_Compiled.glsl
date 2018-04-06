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

struct DirectionalLight
{
    vec4 Direction;
    vec4 DirectionViewSpace;
    vec3 Color;
    float Intensity;
    uint Enabled;
    vec3 Padding;
};

layout(binding = 4, std430) buffer global_aabb
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

layout(binding = 5, std140) uniform dispatch_params
{
    uvec3 NumThreadGroups;
    uvec3 NumThreads;
} DispatchParams;

layout(binding = 6, std140) uniform reduction_params
{
    uint NumElements;
} ReductionParams;

layout(binding = 7, std140) uniform sort_params
{
    uint NumElements;
    uint ChunkSize;
} SortParams;

layout(set = 1, binding = 2, std430) buffer directional_lights
{
    DirectionalLight Data[];
} DirectionalLights;

layout(binding = 0, r32ui) uniform writeonly uimageBuffer PointLightMortonCodes;
layout(binding = 1, r32ui) uniform writeonly uimageBuffer PointLightIndices;
layout(binding = 2, r32ui) uniform writeonly uimageBuffer SpotLightMortonCodes;
layout(binding = 3, r32ui) uniform writeonly uimageBuffer SpotLightIndices;

shared AABB gs_AABB;
shared vec4 gs_AABBRange;

uint MortonCode(uvec3 quantized_coord, uint k)
{
    uint morton_code = 0u;
    uint bit_mask = 1u;
    uint bit_shift = 0u;
    uint k_bits = uint(1 << int(k));
    while (bit_mask < k_bits)
    {
        morton_code |= ((quantized_coord.x & bit_mask) << (bit_shift + 0u));
        morton_code |= ((quantized_coord.y & bit_mask) << (bit_shift + 1u));
        morton_code |= ((quantized_coord.z & bit_mask) << (bit_shift + 2u));
        bit_mask = bit_mask << uint(1);
        bit_shift += 2u;
    }
    return morton_code;
}

void main()
{
    if (gl_LocalInvocationIndex == 0u)
    {
        gs_AABB.Min = LightAABBs.Data[0].Min;
        gs_AABB.Max = LightAABBs.Data[0].Max;
        gs_AABBRange = vec4(1.0) / (gs_AABB.Max - gs_AABB.Min);
    }
    groupMemoryBarrier();
    uint thread_idx = gl_GlobalInvocationID.x;
    if (thread_idx < LightCounts.NumPointLights)
    {
        uvec4 quantized = uvec4(((PointLights.Data[thread_idx].PositionViewSpace - gs_AABB.Min) * gs_AABBRange) * 1023.0);
        uvec3 param = quantized.xyz;
        uint param_1 = 10u;
        imageStore(PointLightMortonCodes, int(thread_idx), uvec4(MortonCode(param, param_1), 0u, 0u, 0u));
        imageStore(PointLightIndices, int(thread_idx), uvec4(thread_idx, 0u, 0u, 0u));
    }
    if (thread_idx < LightCounts.NumSpotLights)
    {
        uvec4 quantized_1 = uvec4(((SpotLights.Data[thread_idx].PositionViewSpace - gs_AABB.Min) * gs_AABBRange) * 1023.0);
        uvec3 param_2 = quantized_1.xyz;
        uint param_3 = 10u;
        imageStore(SpotLightMortonCodes, int(thread_idx), uvec4(MortonCode(param_2, param_3), 0u, 0u, 0u));
        imageStore(SpotLightIndices, int(thread_idx), uvec4(thread_idx, 0u, 0u, 0u));
    }
}

