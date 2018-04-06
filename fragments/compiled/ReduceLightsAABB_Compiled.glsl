#version 450
layout(local_size_x = 512, local_size_y = 1, local_size_z = 1) in;

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

layout(set = 1, binding = 4, std430) buffer global_aabb
{
    AABB Data[];
} LightAABBs;

layout(binding = 3, std140) uniform light_counts
{
    uint NumPointLights;
    uint NumSpotLights;
    uint NumDirectionalLights;
} LightCounts;

layout(binding = 0, std430) buffer point_lights
{
    PointLight Data[];
} PointLights;

layout(set = 1, binding = 5, std140) uniform dispatch_params
{
    uvec3 NumThreadGroups;
    uvec3 NumThreads;
} DispatchParams;

layout(binding = 1, std430) buffer spot_lights
{
    SpotLight Data[];
} SpotLights;

layout(set = 1, binding = 6, std140) uniform reduction_params
{
    uint NumElements;
} ReductionParams;

layout(binding = 2, std430) buffer directional_lights
{
    DirectionalLight Data[];
} DirectionalLights;

layout(set = 1, binding = 7, std140) uniform sort_params
{
    uint NumElements;
    uint ChunkSize;
} SortParams;

layout(set = 1, binding = 0, r32ui) uniform readonly writeonly uimageBuffer PointLightMortonCodes;
layout(set = 1, binding = 1, r32ui) uniform readonly writeonly uimageBuffer PointLightIndices;
layout(set = 1, binding = 2, r32ui) uniform readonly writeonly uimageBuffer SpotLightMortonCodes;
layout(set = 1, binding = 3, r32ui) uniform readonly writeonly uimageBuffer SpotLightIndices;

shared vec4 gs_AABBMin[512u];
shared vec4 gs_AABBMax[512u];

void LogStepReduction()
{
    uint reduceIndex = (512u >> uint(1));
    while (reduceIndex > 32u)
    {
        if (gl_LocalInvocationIndex < reduceIndex)
        {
            gs_AABBMin[gl_LocalInvocationIndex] = min(gs_AABBMin[gl_LocalInvocationIndex], gs_AABBMin[gl_LocalInvocationIndex + reduceIndex]);
            gs_AABBMax[gl_LocalInvocationIndex] = max(gs_AABBMax[gl_LocalInvocationIndex], gs_AABBMax[gl_LocalInvocationIndex + reduceIndex]);
        }
        groupMemoryBarrier();
        reduceIndex = reduceIndex >> uint(1);
    }
    if (gl_LocalInvocationIndex < 32u)
    {
        while (reduceIndex > 0u)
        {
            if (512u >= (reduceIndex << uint(1)))
            {
                gs_AABBMin[gl_LocalInvocationIndex] = min(gs_AABBMin[gl_LocalInvocationIndex], gs_AABBMin[gl_LocalInvocationIndex + reduceIndex]);
                gs_AABBMax[gl_LocalInvocationIndex] = max(gs_AABBMax[gl_LocalInvocationIndex], gs_AABBMax[gl_LocalInvocationIndex + reduceIndex]);
            }
            reduceIndex = reduceIndex >> uint(1);
        }
        if (gl_LocalInvocationIndex == 0u)
        {
            LightAABBs.Data[gl_WorkGroupID.x].Min = gs_AABBMin[gl_LocalInvocationIndex];
            LightAABBs.Data[gl_WorkGroupID.x].Max = gs_AABBMax[gl_LocalInvocationIndex];
        }
    }
}

void reduce0()
{
    vec4 aabb_min = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
    vec4 aabb_max = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
    for (uint i = gl_GlobalInvocationID.x; i < LightCounts.NumPointLights; i += (512u * DispatchParams.NumThreadGroups.x))
    {
        aabb_min = min(aabb_min, PointLights.Data[i].PositionViewSpace - vec4(PointLights.Data[i].Range));
        aabb_max = max(aabb_max, PointLights.Data[i].PositionViewSpace + vec4(PointLights.Data[i].Range));
    }
    for (uint i_1 = gl_GlobalInvocationID.x; i_1 < LightCounts.NumSpotLights; i_1 += (512u * DispatchParams.NumThreadGroups.x))
    {
        aabb_min = min(aabb_min, SpotLights.Data[i_1].PositionViewSpace - vec4(SpotLights.Data[i_1].Range));
        aabb_max = min(aabb_max, SpotLights.Data[i_1].PositionViewSpace + vec4(SpotLights.Data[i_1].Range));
    }
    gs_AABBMin[gl_LocalInvocationIndex] = aabb_min;
    gs_AABBMax[gl_LocalInvocationIndex] = aabb_max;
    groupMemoryBarrier();
    LogStepReduction();
}

void reduce1()
{
    vec4 aabb_min = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
    vec4 aabb_max = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
    for (uint i = gl_LocalInvocationIndex; i < ReductionParams.NumElements; i += (512u * DispatchParams.NumThreadGroups.x))
    {
        aabb_min = min(aabb_min, LightAABBs.Data[i].Min);
        aabb_max = max(aabb_max, LightAABBs.Data[i].Max);
    }
    gs_AABBMin[gl_LocalInvocationIndex] = aabb_min;
    gs_AABBMax[gl_LocalInvocationIndex] = aabb_max;
    groupMemoryBarrier();
    LogStepReduction();
}

void main()
{
    switch (0u)
    {
        case 0:
        {
            reduce0();
            break;
        }
        case 1:
        {
            reduce1();
            break;
        }
    }
}

