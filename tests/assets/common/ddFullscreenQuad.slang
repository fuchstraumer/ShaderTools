implementing ddCommon;

// This file held zero bytes. It is the natural home for the fullscreen triangle that every blit and
// every debug overlay needs, and several passes had no way to draw one.
//
// The shape is one triangle and not two, so there is no seam down the diagonal where two triangles
// meet and the rasterizer runs the edge twice. The triangle covers the whole viewport and the parts
// outside it are clipped.

public struct FullscreenQuadOutput
{
    public float4 ClipPosition: SV_Position;
    public float2 UV: TEXCOORD0;
}

// Gives the clip position and texture coordinate for one of three vertices. Call it with SV_VertexID
// from a draw of three vertices and no vertex buffer.
//
// Vertex 0 gives UV (0,0), vertex 1 gives (2,0), and vertex 2 gives (0,2). The triangle therefore
// reaches twice the size of the viewport on each axis, and the unit square sits in its corner.
public FullscreenQuadOutput GetFullscreenQuadVertex(in const uint vertex_id)
{
    FullscreenQuadOutput output;

    output.UV = float2(float((vertex_id << 1u) & 2u), float(vertex_id & 2u));
    // UV runs down the screen and clip space runs up it, so y takes the opposite sign.
    output.ClipPosition = float4(output.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return output;
}
