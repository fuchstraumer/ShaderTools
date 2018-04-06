#version 450
layout(local_size_x = 512, local_size_y = 1, local_size_z = 1) in;

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

struct AABB
{
    vec4 Min;
    vec4 Max;
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

layout(set = 2, binding = 3, std140) uniform light_counts
{
    uint NumPointLights;
    uint NumSpotLights;
    uint NumDirectionalLights;
} LightCounts;

layout(set = 2, binding = 0, std430) buffer point_lights
{
    PointLight Data[];
} PointLights;

layout(binding = 2, std140) uniform bvh_params
{
    uint PointLightLevels;
    uint SpotLightLevels;
    uint ChildLevel;
} BVHParams;

layout(binding = 0, std430) buffer point_light_bvh
{
    AABB Data[];
} PointLightBVH;

layout(set = 2, binding = 1, std430) buffer spot_lights
{
    SpotLight Data[];
} SpotLights;

layout(binding = 1, std430) buffer spot_light_bvh
{
    AABB Data[];
} SpotLightBVH;

layout(set = 1, binding = 4, std430) buffer global_aabb
{
    AABB Data[];
} LightAABBs;

layout(set = 1, binding = 5, std140) uniform dispatch_params
{
    uvec3 NumThreadGroups;
    uvec3 NumThreads;
} DispatchParams;

layout(set = 1, binding = 6, std140) uniform reduction_params
{
    uint NumElements;
} ReductionParams;

layout(set = 1, binding = 7, std140) uniform sort_params
{
    uint NumElements;
    uint ChunkSize;
} SortParams;

layout(set = 2, binding = 2, std430) buffer directional_lights
{
    DirectionalLight Data[];
} DirectionalLights;

layout(set = 1, binding = 1, r32ui) uniform readonly uimageBuffer PointLightIndices;
layout(set = 1, binding = 3, r32ui) uniform readonly uimageBuffer SpotLightIndices;
layout(set = 1, binding = 0, r32ui) uniform readonly writeonly uimageBuffer PointLightMortonCodes;
layout(set = 1, binding = 2, r32ui) uniform readonly writeonly uimageBuffer SpotLightMortonCodes;

shared vec4 gs_AABBMin[512];
shared vec4 gs_AABBMax[512];

void LogStepReduction()
{
    uint reduceIndex = 16u;
    uint mod32GroupIndex = gl_LocalInvocationIndex % 32u;
    while (mod32GroupIndex < reduceIndex)
    {
        gs_AABBMin[gl_LocalInvocationIndex] = min(gs_AABBMin[gl_LocalInvocationIndex], gs_AABBMin[gl_LocalInvocationIndex + reduceIndex]);
        gs_AABBMax[gl_LocalInvocationIndex] = max(gs_AABBMax[gl_LocalInvocationIndex], gs_AABBMax[gl_LocalInvocationIndex + reduceIndex]);
        reduceIndex = reduceIndex >> uint(1);
    }
}

void BuildBottom()
{
    uint leafIdx = gl_GlobalInvocationID.x;
    uint lightIdx;
    if (leafIdx < LightCounts.NumPointLights)
    {
        lightIdx = imageLoad(PointLightIndices, int(leafIdx)).x;
        gs_AABBMin[gl_LocalInvocationIndex] = PointLights.Data[lightIdx].PositionViewSpace - vec4(PointLights.Data[lightIdx].Range);
        gs_AABBMax[gl_LocalInvocationIndex] = PointLights.Data[lightIdx].PositionViewSpace + vec4(PointLights.Data[lightIdx].Range);
    }
    else
    {
        gs_AABBMin[gl_LocalInvocationIndex] = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
        gs_AABBMax[gl_LocalInvocationIndex] = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
    }
    LogStepReduction();
    uint numLevels;
    uint nodeOffset;
    uint nodeIndex;
    if ((gl_GlobalInvocationID.x % 32u) == 0u)
    {
        numLevels = BVHParams.PointLightLevels;
        nodeOffset = gl_GlobalInvocationID.x / 32u;
        uint _150 = numLevels;
        bool _151 = _150 > 0u;
        bool _171;
        if (_151)
        {
            uint indexable[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
            _171 = nodeOffset < (indexable[numLevels - 1u]);
        }
        else
        {
            _171 = _151;
        }
        if (_171)
        {
            uint indexable_1[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
            nodeIndex = (indexable_1[numLevels - 1u]) + nodeOffset;
            PointLightBVH.Data[nodeIndex].Min = gs_AABBMin[gl_LocalInvocationIndex];
            PointLightBVH.Data[nodeIndex].Max = gs_AABBMax[gl_LocalInvocationIndex];
        }
    }
    if (leafIdx < LightCounts.NumSpotLights)
    {
        lightIdx = imageLoad(SpotLightIndices, int(leafIdx)).x;
        gs_AABBMin[gl_LocalInvocationIndex] = SpotLights.Data[lightIdx].PositionViewSpace - vec4(SpotLights.Data[lightIdx].Range);
        gs_AABBMax[gl_LocalInvocationIndex] = SpotLights.Data[lightIdx].PositionViewSpace + vec4(SpotLights.Data[lightIdx].Range);
    }
    else
    {
        gs_AABBMin[gl_LocalInvocationIndex] = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
        gs_AABBMax[gl_LocalInvocationIndex] = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
    }
    LogStepReduction();
    if ((gl_GlobalInvocationID.x % 32u) == 0u)
    {
        numLevels = BVHParams.SpotLightLevels;
        nodeOffset = gl_GlobalInvocationID.x / 32u;
        uint _258 = numLevels;
        bool _259 = _258 > 0u;
        bool _269;
        if (_259)
        {
            uint indexable_2[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
            _269 = nodeOffset < (indexable_2[numLevels - 1u]);
        }
        else
        {
            _269 = _259;
        }
        if (_269)
        {
            uint indexable_3[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
            nodeIndex = (indexable_3[numLevels - 1u]) + nodeOffset;
            SpotLightBVH.Data[nodeIndex].Min = gs_AABBMin[gl_LocalInvocationIndex];
            SpotLightBVH.Data[nodeIndex].Max = gs_AABBMax[gl_LocalInvocationIndex];
        }
    }
}

void BuildUpper()
{
    bool _298 = BVHParams.ChildLevel < BVHParams.PointLightLevels;
    bool _309;
    if (_298)
    {
        uint indexable[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
        _309 = gl_GlobalInvocationID.x < indexable[BVHParams.ChildLevel];
    }
    else
    {
        _309 = _298;
    }
    if (_309)
    {
        uint indexable_1[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
        uint childIndex = indexable_1[BVHParams.ChildLevel] + gl_GlobalInvocationID.x;
        gs_AABBMin[childIndex] = PointLightBVH.Data[childIndex].Min;
        gs_AABBMax[childIndex] = PointLightBVH.Data[childIndex].Max;
    }
    else
    {
        gs_AABBMin[gl_LocalInvocationIndex] = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
        gs_AABBMax[gl_LocalInvocationIndex] = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
    }
    LogStepReduction();
    if ((gl_GlobalInvocationID.x % 32u) == 0u)
    {
        uint nodeOffset = gl_GlobalInvocationID.x / 32u;
        bool _351 = BVHParams.ChildLevel < BVHParams.PointLightLevels;
        bool _362;
        if (_351)
        {
            uint indexable_2[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
            _362 = nodeOffset < (indexable_2[BVHParams.ChildLevel - 1u]);
        }
        else
        {
            _362 = _351;
        }
        if (_362)
        {
            uint indexable_3[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
            uint nodeIndex = (indexable_3[BVHParams.ChildLevel - 1u]) + nodeOffset;
            PointLightBVH.Data[nodeIndex].Min = gs_AABBMin[gl_LocalInvocationIndex];
            PointLightBVH.Data[nodeIndex].Max = gs_AABBMax[gl_LocalInvocationIndex];
        }
    }
    bool _388 = BVHParams.ChildLevel < BVHParams.SpotLightLevels;
    bool _399;
    if (_388)
    {
        uint indexable_4[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
        _399 = gl_GlobalInvocationID.x < indexable_4[BVHParams.ChildLevel];
    }
    else
    {
        _399 = _388;
    }
    if (_399)
    {
        uint indexable_5[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
        uint childIndex_1 = indexable_5[BVHParams.ChildLevel] + gl_GlobalInvocationID.x;
        gs_AABBMin[childIndex_1] = SpotLightBVH.Data[childIndex_1].Min;
        gs_AABBMax[childIndex_1] = SpotLightBVH.Data[childIndex_1].Max;
    }
    else
    {
        gs_AABBMin[gl_LocalInvocationIndex] = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
        gs_AABBMax[gl_LocalInvocationIndex] = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
    }
    LogStepReduction();
    if ((gl_GlobalInvocationID.x % 32u) == 0u)
    {
        uint nodeOffset_1 = gl_GlobalInvocationID.x / 32u;
        bool _441 = BVHParams.ChildLevel < BVHParams.SpotLightLevels;
        bool _452;
        if (_441)
        {
            uint indexable_6[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
            _452 = nodeOffset_1 < (indexable_6[BVHParams.ChildLevel - 1u]);
        }
        else
        {
            _452 = _441;
        }
        if (_452)
        {
            uint indexable_7[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
            uint nodeIndex_1 = (indexable_7[BVHParams.ChildLevel - 1u]) + nodeOffset_1;
            SpotLightBVH.Data[nodeIndex_1].Min = gs_AABBMin[gl_LocalInvocationIndex];
            SpotLightBVH.Data[nodeIndex_1].Max = gs_AABBMax[gl_LocalInvocationIndex];
        }
    }
}

void main()
{
    switch (0u)
    {
        case 0:
        {
            BuildBottom();
            break;
        }
        case 1:
        {
            BuildUpper();
            break;
        }
    }
}

