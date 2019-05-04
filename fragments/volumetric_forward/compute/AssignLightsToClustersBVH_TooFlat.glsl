#include "Structures.glsl"
#include "Functions.glsl"

#pragma USE_RESOURCES VolumetricForward
#pragma USE_RESOURCES SortResources
#pragma USE_RESOURCES BVHResources
#pragma USE_RESOURCES VolumetricForwardLights

const uint NUM_THREADS = 32u;
layout (local_size_x = NUM_THREADS, local_size_y = 1u, local_size_z = 1u) in;
const uint MaxLights = 2048u;
const uint NODE_STACK_LIMITS = 1024u;

const uint NumChildNodes[6] = {
    1u,
    33u,
    1057u,
    33825u,
    1082401u,
    34636833u
};

// 32 layers of nodes
shared uint gs_NodeStack[1024u]; 
// Current index in the above node stack
shared uint gs_StackPtr;
// The index of the parent node of the BVH node currently being processed
shared uint gs_ParentIdx;

shared uint gs_ClusterIndex1D;
shared AABB gs_ClusterAABB;
shared uint gs_PointLightCount;
shared uint gs_SpotLightCount;
shared uint gs_PointLightStartOffset;
shared uint gs_SpotLightStartOffset;
shared uint gs_PointLightList[MaxLights];
shared uint gs_SpotLightList[MaxLights];
shared bool gs_NonZeroLevels;

void PushNode(in const uint node_index)
{
    atomicAdd(gs_StackPtr, 1);
    if (gs_StackPtr < NODE_STACK_LIMITS)
    {
        gs_NodeStack[gs_StackPtr] = node_index;
    }
}

uint PopNode()
{
    atomicAdd(gs_StackPtr, -1);
    return ((gs_StackPtr != 0u) && (gs_StackPtr < 1024u)) ? gs_NodeStack[gs_StackPtr - 1u] : 0u;
}

// Gets the index of the first node in the current bvh
uint GetFirstChild(in const uint num_levels)
{
    return gs_ParentIdx * 32u + 1u;
}

bool IsLeafNode(in const uint child_index, in const uint num_levels)
{
    return child_index > (NumChildNodes[num_levels - 1u] - 1u);
}

uint GetLeafIndex(in const uint node_index, in const uint num_levels)
{
    return node_index - NumChildNodes[num_levels - 1];
}

void assignPointLights(in const uint childOffset)
{
    uint childIndex = 0u;
    uint leafIndex = 0u;
    Sphere testSphere;

    do
    {
        const uint numLevels = BVHParams.PointLightLevels;
        childIndex = GetFirstChild(numLevels) + childOffset;
        const bool isLeaf = IsLeafNode(childIndex, numLevels);
        const uint leafIndex = GetLeafIndex(childIndex, numLevels);
        const bool validIndex = leafIndex < LightCounts.NumPointLights;
        const bool aabbIntersect = AABBIntersectAABB(gs_ClusterAABB, PointLightBVH.Data[childIndex]);

        if (isLeaf && validIndex)
        {
            const uint lightIndex = imageLoad(PointLightIndices, int(leafIndex)).r;
            const PointLight pointLight = PointLights.Data[lightIndex];
            testSphere.c = pointLight.PositionViewSpace.xyz;
            testSphere.r = pointLight.Range;
            const bool appendLight = SphereInsideAABB_Fast(testSphere, gs_ClusterAABB) && pointLight.Enabled;
            if (appendLight)
            {
                uint idx = atomicAdd(gs_PointLightCount, 1u);
                gs_PointLightList[idx] = lightIndex;
            }
        }
        else if (aabbIntersect)
        {
            PushNode(childIndex);
        }

        groupMemoryBarrier();
        barrier();

        if (childOffset == 0u)
        {
            gs_ParentIdx = PopNode();
        }

        groupMemoryBarrier();
        barrier();

    } while (gs_ParentIdx != 0u);
}

void assignSpotLights(in const uint childOffset)
{
    uint childIndex = 0u;
    uint leafIndex = 0u;
    Sphere testSphere;
    const uint numLevels = BVHParams.PointLightLevels;

    do
    {
        childIndex = GetFirstChild(numLevels) + childOffset;
        const bool isLeaf = IsLeafNode(childIndex, numLevels);
        const uint leafIndex = GetLeafIndex(childIndex, numLevels);
        const bool validIndex = leafIndex < LightCounts.NumSpotLights;
        const bool aabbIntersect = AABBIntersectAABB(gs_ClusterAABB, SpotLightBVH.Data[childIndex]);

        if (isLeaf && validIndex)
        {
            const uint lightIndex = imageLoad(SpotLightIndices, int(leafIndex)).r;
            const SpotLight spotLight = SpotLights.Data[lightIndex];
            testSphere.c = spotLight.PositionViewSpace.xyz;
            testSphere.r = spotLight.Range;
            const bool sphereInsideCluster = SphereInsideAABB_Fast(testSphere, gs_ClusterAABB);
            if (sphereInsideCluster && spotLight.Enabled)
            {
                uint idx = atomicAdd(gs_SpotLightCount, 1u);
                gs_SpotLightList[idx] = lightIndex;
            }
        }
        else if (aabbIntersect)
        {
            PushNode(childIndex);
        }

        groupMemoryBarrier();
        barrier();

        if (childOffset == 0u)
        {
            gs_ParentIdx = PopNode();
        }

        groupMemoryBarrier();
        barrier();

    } while (gs_ParentIdx != 0u);
}

void main() {

    const uint childOffset = gl_LocalInvocationIndex;

    if (childOffset == 0u)
    {
        gs_ParentIdx = 0u;
        gs_StackPtr = 0u;
        gs_PointLightCount = 0u;
        gs_SpotLightCount = 0u;
        gs_ClusterIndex1D = imageLoad(UniqueClusters, int(gl_WorkGroupID.x)).r;
        gs_ClusterAABB.Min = ClusterAABBs.Data[gs_ClusterIndex1D].Min;
        gs_ClusterAABB.Max = ClusterAABBs.Data[gs_ClusterIndex1D].Max;
        gs_NonZeroLevels = BVHParams.PointLightLevels != 0u;
        const uint stack_idx = atomicAdd(gs_StackPtr, 1);
        gs_NodeStack[stack_idx] = 0u;
    }
    
    groupMemoryBarrier();
    barrier();

    if (gs_NonZeroLevels)
    {
        assignPointLights(childOffset);
    }

    groupMemoryBarrier();
    barrier();

    if (childOffset == 0u)
    {
        gs_StackPtr = 0u;
        gs_ParentIdx = 0u;
        gs_NonZeroLevels = BVHParams.SpotLightLevels != 0u;
        const uint stack_idx = atomicAdd(gs_StackPtr, 1);
        gs_NodeStack[stack_idx] = 0u;
    }

    groupMemoryBarrier();
    barrier();

    if (gs_NonZeroLevels)
    {
        assignSpotLights(childOffset);
    }

    groupMemoryBarrier();
    barrier();

    if (childOffset == 0u)
    {
        gs_PointLightStartOffset = imageAtomicAdd(PointLightIndexCounter, int(0), gs_PointLightCount);
        imageStore(PointLightGrid, int(gs_ClusterIndex1D), uvec4(gs_PointLightStartOffset, gs_PointLightCount, 0u, 0u));

        gs_SpotLightStartOffset = imageAtomicAdd(SpotLightIndexCounter, int(0), gs_SpotLightCount);
        imageStore(SpotLightGrid,  int(gs_ClusterIndex1D), uvec4(gs_SpotLightStartOffset, gs_SpotLightCount, 0u, 0u));
    }

    groupMemoryBarrier();
    barrier();
    
    for (uint i = childOffset; i < gs_PointLightCount; i += 32u)
    {
        imageStore(PointLightIndexList, int(gs_PointLightStartOffset + i), uvec4(gs_PointLightList[i], 0u, 0u, 0u));
    }

    for (uint i = childOffset; i < gs_SpotLightCount; i += 32u)
    {
        imageStore(SpotLightIndexList, int(gs_SpotLightStartOffset + i), uvec4(gs_SpotLightList[i], 0u, 0u, 0u));
    }

}
