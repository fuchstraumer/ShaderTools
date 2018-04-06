#version 450
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct AABB
{
    vec4 Min;
    vec4 Max;
};

layout(binding = 7, std140) uniform sort_params
{
    uint NumElements;
    uint ChunkSize;
} SortParams;

layout(binding = 4, std430) buffer global_aabb
{
    AABB Data[];
} LightAABBs;

layout(binding = 5, std140) uniform dispatch_params
{
    uvec3 NumThreadGroups;
    uvec3 NumThreads;
} DispatchParams;

layout(binding = 6, std140) uniform reduction_params
{
    uint NumElements;
} ReductionParams;

layout(set = 1, binding = 1, r32ui) uniform readonly uimageBuffer InputKeys;
layout(set = 1, binding = 2, r32ui) uniform readonly uimageBuffer InputValues;
layout(set = 1, binding = 3, r32ui) uniform writeonly uimageBuffer OutputKeys;
layout(set = 1, binding = 4, r32ui) uniform writeonly uimageBuffer OutputValues;
layout(binding = 0, r32ui) uniform readonly writeonly uimageBuffer PointLightMortonCodes;
layout(binding = 1, r32ui) uniform readonly writeonly uimageBuffer PointLightIndices;
layout(binding = 2, r32ui) uniform readonly writeonly uimageBuffer SpotLightMortonCodes;
layout(binding = 3, r32ui) uniform readonly writeonly uimageBuffer SpotLightIndices;
layout(set = 1, binding = 0, r32i) uniform readonly writeonly iimageBuffer MergePathPartitions;

shared uint gs_Keys[256];
shared uint gs_Values[256];
shared uint gs_E[256];
shared uint gs_F[256];
shared uint gs_TotalFalses;
shared uint gs_D[256];

void main()
{
    uint input_key = imageLoad(InputKeys, int(gl_LocalInvocationID.x)).x;
    uint input_value = imageLoad(InputValues, int(gl_LocalInvocationID.x)).x;
    gs_Keys[gl_LocalInvocationIndex] = (gl_LocalInvocationID.x < SortParams.NumElements) ? input_key : 4294967295u;
    gs_Values[gl_LocalInvocationIndex] = (gl_LocalInvocationID.x < SortParams.NumElements) ? input_value : 4294967295u;
    for (uint b = 0u; b < 30u; b++)
    {
        gs_E[gl_LocalInvocationIndex] = uint(int(((gs_Keys[gl_LocalInvocationIndex] >> b) & 1u) == 0u));
        groupMemoryBarrier();
        if (gl_LocalInvocationIndex == 0u)
        {
            gs_F[gl_LocalInvocationIndex] = 0u;
        }
        else
        {
            gs_F[gl_LocalInvocationIndex] = gs_E[gl_LocalInvocationIndex - 1u];
        }
        groupMemoryBarrier();
        for (uint i = 1u; i < 256u; i = i << uint(1))
        {
            uint tmp = gs_F[gl_LocalInvocationIndex];
            if (gl_LocalInvocationIndex > i)
            {
                tmp += (gs_F[gl_LocalInvocationIndex - i]);
            }
            groupMemoryBarrier();
            gs_F[gl_LocalInvocationIndex] = tmp;
            groupMemoryBarrier();
        }
        if (gl_LocalInvocationIndex == 0u)
        {
            gs_TotalFalses = gs_E[255] + gs_F[255];
        }
        groupMemoryBarrier();
        uint _150;
        if (gs_E[gl_LocalInvocationIndex] == 1u)
        {
            _150 = gs_F[gl_LocalInvocationIndex];
        }
        else
        {
            _150 = (gl_LocalInvocationIndex - gs_F[gl_LocalInvocationIndex]) + gs_TotalFalses;
        }
        gs_D[gl_LocalInvocationIndex] = _150;
        uint key = gs_Keys[gl_LocalInvocationIndex];
        uint value = gs_Keys[gl_LocalInvocationIndex];
        groupMemoryBarrier();
        gs_Keys[gs_D[gl_LocalInvocationIndex]] = key;
        gs_Values[gs_D[gl_LocalInvocationIndex]] = value;
        groupMemoryBarrier();
    }
    imageStore(OutputKeys, int(gl_LocalInvocationID.x), uvec4(gs_Keys[gl_LocalInvocationIndex], 0u, 0u, 0u));
    imageStore(OutputValues, int(gl_LocalInvocationID.x), uvec4(gs_Values[gl_LocalInvocationIndex], 0u, 0u, 0u));
}

