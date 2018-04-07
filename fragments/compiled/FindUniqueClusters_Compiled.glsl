#version 450
layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, r8ui) uniform readonly uimageBuffer ClusterFlags;
layout(set = 0, binding = 7, r32ui) uniform uimageBuffer UniqueClustersCounter;
layout(set = 0, binding = 8, r32ui) uniform writeonly uimageBuffer UniqueClusters;

void main()
{
    int _17 = int(gl_GlobalInvocationID.x);
    if (imageLoad(ClusterFlags, _17).x == 1u)
    {
        uint _39 = imageAtomicAdd(UniqueClustersCounter, 0, 1u);
        imageStore(UniqueClusters, int(_39), uvec4(uint(_17), 0u, 0u, 0u));
    }
}

