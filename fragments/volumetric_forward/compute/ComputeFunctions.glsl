
uvec3 IdxToCoord(uint idx) {
    return uvec3(
        idx % ClusterData.GridDim.x,
        idx % (ClusterData.GridDim.x * ClusterData.GridDim.y) / ClusterData.GridDim.x,
        idx / (ClusterData.GridDim.x * ClusterData.GridDim.y
    );
}

uint CoordToIdx(uvec3 coord) {
    return coord.x + (ClusterData.GridDim.x * (coord.y + ClusterData.GridDim.y * coord.z));
}

uvec3 ComputeClusterIndex3D(vec2 screen_pos, float view_z) {
    uint i = int(screen_pos.x / ClusterData.Size.x);
    uint j = int(screen_pos.y / ClusterData.Size.y);
    uint k = log(-view_z / ClusterData.ViewNear) * ClusterData.LogGridDimY;
    return uvec3(i, j, k);
}

uint GetGroupIndex() {
    return gl_WorkGroupID.x + (gl_WorkGroupSize.x * (gl_WorkGroupID.y + gl_WorkGroupSize.y * gl_WorkGroupID.z));
}
