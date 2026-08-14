module VertexFormats;

// The raster probe. VeloxRhi has no raster shader asset, so nothing tells the cooker what a vertex
// input layout or a color target reflects as. This module gives it one of each.
//
// The vertex format is the one VeloxRhi proposes: a compact position, one quaternion tangent that
// carries the whole tangent frame, and a texture coordinate pair.

struct VertexInput
{
    half4 position : POSITION;
    float4 qTangent : TANGENT;
    float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 clipPosition : SV_Position;
    float4 qTangent : TANGENT;
    float2 uv : TEXCOORD0;
};

struct SceneUniforms
{
    float4x4 ViewProjection;
    float4 BaseColorFactor;
};

uniform ConstantBuffer<SceneUniforms> Scene;
uniform Texture2D<float4> BaseColorTexture;
uniform SamplerState BaseColorSampler;

[shader("vertex")]
VertexOutput MainVS(VertexInput input)
{
    VertexOutput output;
    output.clipPosition = mul(Scene.ViewProjection, float4(float3(input.position.xyz), 1.0f));
    output.qTangent = input.qTangent;
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 MainFS(VertexOutput input) : SV_Target
{
    const float4 sampled = BaseColorTexture.Sample(BaseColorSampler, input.uv);
    return sampled * Scene.BaseColorFactor;
}
