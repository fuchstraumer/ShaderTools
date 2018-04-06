#version 450
layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

struct AABB
{
    vec4 Min;
    vec4 Max;
};

layout(binding = 9, std430) buffer cluster_aabbs
{
    AABB Data[];
} ClusterAABBs;

layout(binding = 10, std140) uniform cluster_data
{
    uvec3 GridDim;
    float ViewNear;
    uvec2 Size;
    float Near;
    float LogGridDimY;
} ClusterData;

layout(binding = 0, r8ui) uniform readonly uimageBuffer ClusterFlags;
layout(binding = 7, r32ui) uniform uimageBuffer UniqueClustersCounter;
layout(binding = 8, r32ui) uniform writeonly uimageBuffer UniqueClusters;
layout(binding = 1, r32ui) uniform readonly writeonly uimageBuffer PointLightIndexList;
layout(binding = 2, r32ui) uniform readonly writeonly uimageBuffer SpotLightIndexList;
layout(binding = 3, rg32ui) uniform readonly writeonly uimageBuffer PointLightGrid;
layout(binding = 4, rg32ui) uniform readonly writeonly uimageBuffer SpotLightGrid;
layout(binding = 5, r32ui) uniform readonly writeonly uimageBuffer PointLightIndexCounter;
layout(binding = 6, r32ui) uniform readonly writeonly uimageBuffer SpotLightIndexCounter;

void main()
{
    int idx = int(gl_GlobalInvocationID.x);
    if (imageLoad(ClusterFlags, idx).x == 1u)
    {
        uint _39 = imageAtomicAdd(UniqueClustersCounter, 0, 1u);
        uint unique_idx = _39;
        imageStore(UniqueClusters, int(unique_idx), uvec4(uint(idx), 0u, 0u, 0u));
    }
}

