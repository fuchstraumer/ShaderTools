#version 450
layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

struct Sphere
{
    vec3 c;
    float r;
};

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

layout(binding = 9, std430) buffer cluster_aabbs
{
    AABB Data[];
} ClusterAABBs;

layout(set = 2, binding = 2, std140) uniform bvh_params
{
    uint PointLightLevels;
    uint SpotLightLevels;
    uint ChildLevel;
} BVHParams;

layout(set = 3, binding = 3, std140) uniform light_counts
{
    uint NumPointLights;
    uint NumSpotLights;
    uint NumDirectionalLights;
} LightCounts;

layout(set = 3, binding = 0, std430) buffer point_lights
{
    PointLight Data[];
} PointLights;

layout(set = 2, binding = 0, std430) buffer point_light_bvh
{
    AABB Data[];
} PointLightBVH;

layout(set = 3, binding = 1, std430) buffer spot_lights
{
    SpotLight Data[];
} SpotLights;

layout(set = 2, binding = 1, std430) buffer spot_light_bvh
{
    AABB Data[];
} SpotLightBVH;

layout(binding = 10, std140) uniform cluster_data
{
    uvec3 GridDim;
    float ViewNear;
    uvec2 Size;
    float Near;
    float LogGridDimY;
} ClusterData;

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

layout(set = 3, binding = 2, std430) buffer directional_lights
{
    DirectionalLight Data[];
} DirectionalLights;

layout(binding = 8, r32ui) uniform readonly uimageBuffer UniqueClusters;
layout(set = 1, binding = 1, r32ui) uniform readonly uimageBuffer PointLightIndices;
layout(set = 1, binding = 3, r32ui) uniform readonly uimageBuffer SpotLightIndices;
layout(binding = 5, r32ui) uniform uimageBuffer PointLightIndexCounter;
layout(binding = 3, rg32ui) uniform writeonly uimageBuffer PointLightGrid;
layout(binding = 6, r32ui) uniform uimageBuffer SpotLightIndexCounter;
layout(binding = 4, rg32ui) uniform writeonly uimageBuffer SpotLightGrid;
layout(binding = 1, r32ui) uniform writeonly uimageBuffer PointLightIndexList;
layout(binding = 2, r32ui) uniform writeonly uimageBuffer SpotLightIndexList;
layout(binding = 0, r8ui) uniform readonly writeonly uimageBuffer ClusterFlags;
layout(binding = 7, r32ui) uniform readonly writeonly uimageBuffer UniqueClustersCounter;
layout(set = 1, binding = 0, r32ui) uniform readonly writeonly uimageBuffer PointLightMortonCodes;
layout(set = 1, binding = 2, r32ui) uniform readonly writeonly uimageBuffer SpotLightMortonCodes;

shared uint gs_SpotLightCount;
shared uint gs_SpotLightList[1024u];
shared uint gs_PointLightCount;
shared uint gs_PointLightList[1024u];
shared uint gs_StackPtr;
shared uint gs_NodeStack[1024];
shared uint gs_ParentIdx;
shared uint gs_ClusterIndex1D;
shared AABB gs_ClusterAABB;
shared uint gs_PointLightStartOffset;
shared uint gs_SpotLightStartOffset;

void PushNode(uint node_index)
{
    uint _187 = atomicAdd(gs_StackPtr, 1u);
    uint stack_idx = _187;
    if (stack_idx < 1024u)
    {
        gs_NodeStack[stack_idx] = node_index;
    }
}

uint GetFirstChild(uint parent_index, uint num_levels)
{
    uint _219;
    if (num_levels > 0u)
    {
        _219 = (parent_index * 32u) + 1u;
    }
    else
    {
        _219 = 0u;
    }
    return _219;
}

bool IsLeafNode(uint child_index, uint num_levels)
{
    bool _233;
    if (num_levels > 0u)
    {
        uint indexable[6] = uint[](1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
        _233 = child_index > (indexable[num_levels - 1u]);
    }
    else
    {
        _233 = true;
    }
    return _233;
}

uint GetLeafIndex(uint node_index, uint num_levels)
{
    uint _259;
    if (num_levels > 0u)
    {
        uint indexable[6] = uint[](1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
        _259 = node_index - (indexable[num_levels - 1u]);
    }
    else
    {
        _259 = node_index;
    }
    return _259;
}

float SqDistanceFromPtToAABB(vec3 p, AABB b)
{
    float result = 0.0;
    for (int i = 0; i < 3; i++)
    {
        if (p[i] < b.Min[i])
        {
            result += pow(b.Min[i] - p[i], 2.0);
        }
        if (p[i] > b.Max[i])
        {
            result += pow(p[i] - b.Max[i], 2.0);
        }
    }
    return result;
}

bool SphereInsideAABB(Sphere sphere, AABB b)
{
    vec3 param = sphere.c;
    AABB param_1 = b;
    float sq_distance = SqDistanceFromPtToAABB(param, param_1);
    return sq_distance <= (sphere.r * sphere.r);
}

void AppendPointLight(uint light_index)
{
    uint _176 = atomicAdd(gs_PointLightCount, 1u);
    uint idx = _176;
    if (idx < 1024u)
    {
        gs_PointLightList[idx] = light_index;
    }
}

bool AABBIntersectAABB(AABB a, AABB b)
{
    bvec4 result0 = greaterThan(a.Max, b.Min);
    bvec4 result1 = lessThan(a.Min, b.Max);
    return all(result0) && all(result1);
}

uint PopNode()
{
    uint node_idx = 0u;
    uint _202 = atomicAdd(gs_StackPtr, 4294967295u);
    uint stack_idx = _202;
    if ((stack_idx > 0u) && (stack_idx < 1024u))
    {
        node_idx = gs_NodeStack[stack_idx - 1u];
    }
    return node_idx;
}

void AppendSpotLight(uint light_index)
{
    uint _162 = atomicAdd(gs_SpotLightCount, 1u);
    uint idx = _162;
    if (idx < 1024u)
    {
        gs_SpotLightList[idx] = light_index;
    }
}

void main()
{
    if (gl_LocalInvocationIndex == 0u)
    {
        gs_PointLightCount = 0u;
        gs_SpotLightCount = 0u;
        gs_StackPtr = 0u;
        gs_ParentIdx = 0u;
        gs_ClusterIndex1D = imageLoad(UniqueClusters, int(gl_WorkGroupID.x)).x;
        gs_ClusterAABB.Min = ClusterAABBs.Data[gs_ClusterIndex1D].Min;
        gs_ClusterAABB.Max = ClusterAABBs.Data[gs_ClusterIndex1D].Max;
        uint param = 0u;
        PushNode(param);
    }
    groupMemoryBarrier();
    do
    {
        uint param_1 = gs_ParentIdx;
        uint param_2 = BVHParams.PointLightLevels;
        uint child_index = GetFirstChild(param_1, param_2) + gl_LocalInvocationIndex;
        uint param_3 = child_index;
        uint param_4 = BVHParams.PointLightLevels;
        if (IsLeafNode(param_3, param_4))
        {
            uint param_5 = child_index;
            uint param_6 = BVHParams.PointLightLevels;
            uint leaf_index = GetLeafIndex(param_5, param_6);
            if (leaf_index < LightCounts.NumPointLights)
            {
                uint light_index = imageLoad(PointLightIndices, int(leaf_index)).x;
                Sphere sphere;
                sphere.c = PointLights.Data[light_index].PositionViewSpace.xyz;
                sphere.r = PointLights.Data[light_index].Range;
                uint _384 = PointLights.Data[light_index].Enabled;
                bool _385 = _384 != 0u;
                bool _393;
                if (_385)
                {
                    Sphere param_7 = sphere;
                    AABB param_8 = gs_ClusterAABB;
                    _393 = SphereInsideAABB(param_7, param_8);
                }
                else
                {
                    _393 = _385;
                }
                if (_393)
                {
                    uint param_9 = light_index;
                    AppendPointLight(param_9);
                }
            }
        }
        else
        {
            AABB param_10 = gs_ClusterAABB;
            AABB param_11;
            param_11.Min = PointLightBVH.Data[child_index].Min;
            param_11.Max = PointLightBVH.Data[child_index].Max;
            if (AABBIntersectAABB(param_10, param_11))
            {
                uint param_12 = child_index;
                PushNode(param_12);
            }
        }
        groupMemoryBarrier();
        if (gl_LocalInvocationIndex == 0u)
        {
            uint _426 = PopNode();
            gs_ParentIdx = _426;
        }
        groupMemoryBarrier();
    } while (gs_ParentIdx > 0u);
    groupMemoryBarrier();
    if (gl_LocalInvocationIndex == 0u)
    {
        gs_StackPtr = 0u;
        gs_ParentIdx = 0u;
        uint param_13 = 0u;
        PushNode(param_13);
    }
    groupMemoryBarrier();
    do
    {
        uint param_14 = gs_ParentIdx;
        uint param_15 = BVHParams.SpotLightLevels;
        uint child_index_1 = GetFirstChild(param_14, param_15) + gl_LocalInvocationIndex;
        uint param_16 = child_index_1;
        uint param_17 = BVHParams.SpotLightLevels;
        if (IsLeafNode(param_16, param_17))
        {
            uint param_18 = child_index_1;
            uint param_19 = BVHParams.SpotLightLevels;
            uint leaf_index_1 = GetLeafIndex(param_18, param_19);
            if (leaf_index_1 < LightCounts.NumSpotLights)
            {
                uint light_index_1 = imageLoad(SpotLightIndices, int(leaf_index_1)).x;
                Sphere sphere_1;
                sphere_1.c = SpotLights.Data[light_index_1].PositionViewSpace.xyz;
                sphere_1.r = SpotLights.Data[light_index_1].Range;
                uint _495 = SpotLights.Data[light_index_1].Enabled;
                bool _496 = _495 != 0u;
                bool _504;
                if (_496)
                {
                    Sphere param_20 = sphere_1;
                    AABB param_21 = gs_ClusterAABB;
                    _504 = SphereInsideAABB(param_20, param_21);
                }
                else
                {
                    _504 = _496;
                }
                if (_504)
                {
                    uint param_22 = light_index_1;
                    AppendSpotLight(param_22);
                }
            }
        }
        else
        {
            AABB param_23 = gs_ClusterAABB;
            AABB param_24;
            param_24.Min = SpotLightBVH.Data[child_index_1].Min;
            param_24.Max = SpotLightBVH.Data[child_index_1].Max;
            if (AABBIntersectAABB(param_23, param_24))
            {
                uint param_25 = child_index_1;
                PushNode(param_25);
            }
        }
        groupMemoryBarrier();
        if (gl_LocalInvocationIndex == 0u)
        {
            uint _535 = PopNode();
            gs_ParentIdx = _535;
        }
        groupMemoryBarrier();
    } while (gs_ParentIdx > 0u);
    if (gl_LocalInvocationIndex == 0u)
    {
        uint _547 = imageAtomicAdd(PointLightIndexCounter, 0, gs_PointLightCount);
        gs_PointLightStartOffset = _547;
        imageStore(PointLightGrid, int(gs_ClusterIndex1D), uvec4(gs_PointLightStartOffset, gs_PointLightCount, 0u, 0u));
        uint _561 = imageAtomicAdd(SpotLightIndexCounter, 0, gs_SpotLightCount);
        gs_SpotLightStartOffset = _561;
        imageStore(SpotLightGrid, int(gs_ClusterIndex1D), uvec4(gs_SpotLightStartOffset, gs_SpotLightCount, 0u, 0u));
    }
    groupMemoryBarrier();
    for (int i = int(gl_LocalInvocationIndex); uint(i) < gs_PointLightCount; i += 32)
    {
        imageStore(PointLightIndexList, int(gs_PointLightStartOffset + uint(i)), uvec4(gs_PointLightList[i], 0u, 0u, 0u));
    }
    for (int i_1 = int(gl_LocalInvocationIndex); uint(i_1) < gs_SpotLightCount; i_1 += 32)
    {
        imageStore(SpotLightIndexList, int(gs_SpotLightStartOffset + uint(i_1)), uvec4(gs_SpotLightList[i_1], 0u, 0u, 0u));
    }
}

