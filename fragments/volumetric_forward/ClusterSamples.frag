#include "ComputeFunctions.glsl"

void main() {
    vec4 vpos = matrices.View * vPosition;
    uvec3 cluster_index_3d = ComputeClusterIndex3D(gl_FragCoord.xy, vpos.z);
    uint idx = ComputeClusterIndex1D(cluster_index_3d);
    imageStore(Flags, int(idx), uvec4(1, 0, 0, 0));
}
