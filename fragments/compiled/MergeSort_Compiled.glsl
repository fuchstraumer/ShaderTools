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
layout(set = 1, binding = 3, r32ui) uniform writeonly uimageBuffer OutputKeys;
layout(set = 1, binding = 4, r32ui) uniform writeonly uimageBuffer OutputValues;
layout(set = 1, binding = 0, r32i) uniform iimageBuffer MergePathPartitions;
layout(set = 1, binding = 2, r32ui) uniform readonly uimageBuffer InputValues;
layout(binding = 0, r32ui) uniform readonly writeonly uimageBuffer PointLightMortonCodes;
layout(binding = 1, r32ui) uniform readonly writeonly uimageBuffer PointLightIndices;
layout(binding = 2, r32ui) uniform readonly writeonly uimageBuffer SpotLightMortonCodes;
layout(binding = 3, r32ui) uniform readonly writeonly uimageBuffer SpotLightIndices;

shared uint gs_Keys[2048];
shared uint gs_Values[2048];

int MergePath(int a0, int aCount, int b0, int bCount, int diag, bool use_shared_memory)
{
    int begin = max(0, diag - bCount);
    int end = min(diag, aCount);
    while (begin < end)
    {
        int mid = (begin + end) >> 1;
        uint _61;
        if (use_shared_memory)
        {
            _61 = gs_Keys[a0 + mid];
        }
        else
        {
            _61 = imageLoad(InputKeys, a0 + mid).x;
        }
        uint a = _61;
        uint _89;
        if (use_shared_memory)
        {
            _89 = gs_Keys[((b0 + diag) - 1) - mid];
        }
        else
        {
            _89 = imageLoad(InputKeys, ((b0 + diag) - 1) - mid).x;
        }
        uint b = _89;
        if (a < b)
        {
            begin = mid + 1;
        }
        else
        {
            end = mid;
        }
    }
    return begin;
}

void MergePathPartitionsFunc()
{
    uint chunkSize = SortParams.ChunkSize;
    uint numValuesPerSortGroup = min(chunkSize * 2u, SortParams.NumElements);
    uint numChunks = uint(ceil(float(SortParams.NumElements) / float(chunkSize)));
    uint numSortGroups = numChunks / 2u;
    uint numPartitionsPerSortGroup = uint(ceil(float(numValuesPerSortGroup) / 2048.0) + 1.0);
    uint sortGroup = gl_GlobalInvocationID.x / numPartitionsPerSortGroup;
    uint partitionInSortGroup = gl_GlobalInvocationID.x % numPartitionsPerSortGroup;
    uint globalPartition = (sortGroup * numPartitionsPerSortGroup) + partitionInSortGroup;
    uint maxPartitions = numSortGroups * numPartitionsPerSortGroup;
    if (globalPartition < maxPartitions)
    {
        int a0 = int(sortGroup * numValuesPerSortGroup);
        int a1 = int(min(uint(a0) + chunkSize, SortParams.NumElements));
        int aCount = a1 - a0;
        int b0 = a1;
        int b1 = int(min(uint(b0) + chunkSize, SortParams.NumElements));
        int bCount = b1 - b0;
        int numValues = aCount + bCount;
        int diag = int(min(partitionInSortGroup * 2048u, uint(numValues)));
        int param = a0;
        int param_1 = aCount;
        int param_2 = b0;
        int param_3 = bCount;
        int param_4 = diag;
        bool param_5 = false;
        int mergePath = MergePath(param, param_1, param_2, param_3, param_4, param_5);
        imageStore(MergePathPartitions, int(globalPartition), ivec4(mergePath, 0, 0, 0));
    }
}

void SerialMerge(inout int a0, int a1, inout int b0, int b1, int diag, int num_values, int out0)
{
    uint aKey = gs_Keys[a0];
    uint bKey = gs_Keys[b0];
    uint aValue = gs_Values[a0];
    uint bValue = gs_Values[b0];
    int i = 0;
    for (;;)
    {
        int _146 = i;
        bool _149 = uint(_146) < 8u;
        bool _157;
        if (_149)
        {
            _157 = (diag + i) < num_values;
        }
        else
        {
            _157 = _149;
        }
        if (_157)
        {
            int idx = (out0 + diag) + i;
            int _164 = b0;
            int _165 = b1;
            bool _166 = _164 >= _165;
            bool _177;
            if (!_166)
            {
                _177 = (a0 < a1) && (aKey < bKey);
            }
            else
            {
                _177 = _166;
            }
            if (_177)
            {
                imageStore(OutputKeys, idx, uvec4(aKey, 0u, 0u, 0u));
                imageStore(OutputValues, idx, uvec4(aValue, 0u, 0u, 0u));
                a0++;
                aKey = gs_Keys[a0];
                aValue = gs_Values[a0];
            }
            else
            {
                imageStore(OutputKeys, idx, uvec4(bKey, 0u, 0u, 0u));
                imageStore(OutputValues, idx, uvec4(bValue, 0u, 0u, 0u));
                b0++;
                bKey = gs_Keys[b0];
                bValue = gs_Values[b0];
            }
            i++;
            continue;
        }
        else
        {
            break;
        }
    }
}

void MergeSort()
{
    uint numChunks = uint(ceil(float(SortParams.NumElements) / float(SortParams.ChunkSize)));
    uint numSortGroups = max(numChunks / 2u, 1u);
    uint numValuesPerSortGroup = min(SortParams.ChunkSize * 2u, SortParams.NumElements);
    uint numThreadGroupsPerSortGroup = uint(ceil(float(numValuesPerSortGroup) / 2048.0));
    uint numPartitionsPerSortGroup = numThreadGroupsPerSortGroup + 1u;
    uint sortGroup = gl_GlobalInvocationID.x / numThreadGroupsPerSortGroup;
    uint partitionVal = gl_GlobalInvocationID.x % numThreadGroupsPerSortGroup;
    uint globalPartition = (sortGroup * numPartitionsPerSortGroup) + partitionVal;
    int mergePath0 = imageLoad(MergePathPartitions, int(globalPartition)).x;
    int mergePath1 = imageLoad(MergePathPartitions, int(globalPartition + 1u)).x;
    int diag0 = int(min(partitionVal * 2048u, numValuesPerSortGroup));
    int diag1 = int(min((partitionVal + 1u) * 2048u, numValuesPerSortGroup));
    int chunkOffsetA0 = int(min(sortGroup * numValuesPerSortGroup, SortParams.NumElements));
    int chunkOffsetA1 = int(min(uint(chunkOffsetA0) + SortParams.ChunkSize, SortParams.NumElements));
    int chunkSizeA = chunkOffsetA1 - chunkOffsetA0;
    int chunkOffsetB1 = int(min(uint(chunkOffsetA1) + SortParams.ChunkSize, SortParams.NumElements));
    int chunkSizeB = chunkOffsetB1 - chunkOffsetA1;
    uint numValues = uint(chunkSizeA + chunkSizeB);
    int numA = min(mergePath1 - mergePath0, chunkSizeA);
    int b0 = diag0 - mergePath0;
    int b1 = diag1 - mergePath1;
    int numB = min(b1 - b0, chunkSizeB);
    int diag = int(gl_LocalInvocationIndex) * 8;
    for (int i = 0; uint(i) < 8u; i++)
    {
        int a = (mergePath0 + diag) + i;
        int b = b0 + (a - mergePath1);
        uint key;
        uint value;
        if (a < mergePath1)
        {
            key = imageLoad(InputKeys, chunkOffsetA0 + a).x;
            value = imageLoad(InputValues, chunkOffsetA0 + a).x;
        }
        else
        {
            key = imageLoad(InputKeys, chunkOffsetA1 + b).x;
            key = imageLoad(InputKeys, chunkOffsetA1 + b).x;
        }
        gs_Keys[diag + i] = key;
        gs_Values[diag + i] = value;
    }
    groupMemoryBarrier();
    int param = 0;
    int param_1 = numA;
    int param_2 = numA;
    int param_3 = numB;
    int param_4 = diag;
    bool param_5 = true;
    int mergePath = MergePath(param, param_1, param_2, param_3, param_4, param_5);
    int param_6 = mergePath;
    int param_7 = numA;
    int param_8 = (numA + diag) - mergePath;
    int param_9 = numA + numB;
    int param_10 = diag0 + diag;
    int param_11 = int(numValues);
    int param_12 = chunkOffsetA0;
    SerialMerge(param_6, param_7, param_8, param_9, param_10, param_11, param_12);
}

void main()
{
    switch (0u)
    {
        case 0:
        {
            MergePathPartitionsFunc();
            break;
        }
        case 1:
        {
            MergeSort();
            break;
        }
    }
}

