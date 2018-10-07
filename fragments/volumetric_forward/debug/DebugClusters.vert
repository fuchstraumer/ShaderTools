
#pragma INTERFACE_OVERRIDE
layout (location = 0) out vec4 vMin;
layout (location = 1) out vec4 vMax;
layout (location = 2) out vec4 vColor;
#pragma END_INTERFACE_OVERRIDE

#pragma USE_RESOURCES GLOBAL_RESOURCES
#pragma USE_RESOURCES VOLUMETRIC_FORWARD
#pragma USE_RESOURCES DEBUG

void main() {
    uint cluster_index = imageLoad(UniqueClusters, int(gl_VertexID));
    vMin = ClusterAABBs.Data[cluster_index].Min;
    vMax = ClusterAABBs.Data[cluster_index].Max;
    vec4 out_color = imageLoad(ClusterColors, cluster_index);
    vColor = out_color;
}
