layout (points) in;
layout (triangle_strip, max_vertices = 16) out;
#pragma INTERFACE_OVERRIDE
layout (location = 0) in vec4 vMin;
layout (location = 1) in vec4 vMax;
layout (location = 2) in vec4 vColor;
layout (location = 0) out vec4 gColor;
#pragma END_INTERFACE_OVERRIDE

#pragma USE_RESOURCES GLOBAL_RESOURCES

out gl_PerVertex {
    vec4 gl_Position;
};

const vec4 base_colors[8] = {
    float4( 0.0f, 0.0f, 0.0f, 1.0f ),       // Black
    float4( 0.0f, 0.0f, 1.0f, 1.0f ),       // Blue
    float4( 0.0f, 1.0f, 0.0f, 1.0f ),       // Green
    float4( 0.0f, 1.0f, 1.0f, 1.0f ),       // Cyan
    float4( 1.0f, 0.0f, 0.0f, 1.0f ),       // Red
    float4( 1.0f, 0.0f, 1.0f, 1.0f ),       // Magenta
    float4( 1.0f, 1.0f, 0.0f, 1.0f ),       // Yellow
    float4( 1.01, 1.0f, 1.0f, 1.0f )        // White
};

const uint Index[18] = {
    0, 1, 2,
    3, 6, 7,
    4, 5, -1,
    2, 6, 0,
    4, 1, 5,
    3, 7, -1
};

void main() {

    // AABB vertices
    const vec4 Pos[8] = {
        vec4( vMin.x, vMin.y, vMin.z, 1.0f ),    // 0
        vec4( vMin.x, vMin.y, vMax.z, 1.0f ),    // 1
        vec4( vMin.x, vMax.y, vMin.z, 1.0f ),    // 2
        vec4( vMin.x, vMax.y, vMax.z, 1.0f ),    // 3
        vec4( vMax.x, vMin.y, vMin.z, 1.0f ),    // 4
        vec4( vMax.x, vMin.y, vMax.z, 1.0f ),    // 5
        vec4( vMax.x, vMax.y, vMin.z, 1.0f ),    // 6
        vec4( vMax.x, vMax.y, vMax.z, 1.0f )     // 7
    };

    [[unroll]]
    for (uint i = 0; i < 18; ++i) {
        [[flatten]]
        if (indices[i] == (uint)-1) {
            EndPrimitive();
        }
        else {
            gl_Position = matrices.projection * matrices.view * Pos[indices[i]];
            gColor = vColor;
            EmitVertex();
        }
    }

}
