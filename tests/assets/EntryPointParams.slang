module EntryPointParams;
import LodestoneAttributes;

// A probe for the entry point parameter scope, and the acceptance test for phase E step E0a.
//
// The global scope declares one storage buffer. Each entry point then declares its own `uniform`
// parameters. Slang gives an entry point parameter no binding in the global scope, so a cooker that
// reads the global scope alone misses every one of them.
//
// Two entry points declare a texture and a sampler at the same local placement. Slang generates each
// entry point as its own artifact, so a placement query cannot tell the two apart. The module
// therefore proves that ownership decides visibility, and a placement query does not.
//
// The module registers no permutation space, so it cooks as one variant.

RWStructuredBuffer<float4> Output;

[shader("compute")]
[numthreads(64, 1, 1)]
void ShadeCS(uint3 threadId: SV_DispatchThreadID,
             uniform Texture2D<float4> albedoMap,
             uniform SamplerState albedoSampler)
{
    const float2 uv = float2(threadId.xy) * (1.0f / 256.0f);
    Output[threadId.x] = albedoMap.SampleLevel(albedoSampler, uv, 0.0f);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void TintCS(uint3 threadId: SV_DispatchThreadID,
            uniform Texture2D<float4> tintMap,
            uniform SamplerState tintSampler)
{
    const float2 uv = float2(threadId.xy) * (1.0f / 128.0f);
    Output[threadId.x] *= tintMap.SampleLevel(tintSampler, uv, 0.0f);
}

// This entry point declares no parameter. Its visibility list must hold the global buffer alone.
[shader("compute")]
[numthreads(64, 1, 1)]
void ClearCS(uint3 threadId: SV_DispatchThreadID)
{
    Output[threadId.x] = float4(0.0f, 0.0f, 0.0f, 0.0f);
}
