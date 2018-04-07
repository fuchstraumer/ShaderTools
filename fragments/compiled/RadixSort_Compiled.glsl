#version 450
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 7, std140) uniform sort_params
{
    uint NumElements;
    uint ChunkSize;
} SortParams;

layout(set = 1, binding = 1, r32ui) uniform readonly uimageBuffer InputKeys;
layout(set = 1, binding = 2, r32ui) uniform readonly uimageBuffer InputValues;
layout(set = 1, binding = 3, r32ui) uniform writeonly uimageBuffer OutputKeys;
layout(set = 1, binding = 4, r32ui) uniform writeonly uimageBuffer OutputValues;

shared uint gs_Keys[256];
shared uint gs_Values[256];
shared uint gs_E[256];
shared uint gs_F[256];
shared uint gs_TotalFalses;
shared uint gs_D[256];

void main()
{
    int _21 = int(gl_LocalInvocationID.x);
    bool _49 = gl_LocalInvocationID.x < SortParams.NumElements;
    gs_Keys[gl_LocalInvocationIndex] = _49 ? imageLoad(InputKeys, _21).x : 4294967295u;
    gs_Values[gl_LocalInvocationIndex] = _49 ? imageLoad(InputValues, _21).x : 4294967295u;
    uint _169;
    uint _173;
    uint _226;
    for (uint _224 = 0u; _224 < 30u; gs_D[gl_LocalInvocationIndex] = _226, _169 = gs_Keys[gl_LocalInvocationIndex], _173 = gs_Keys[gl_LocalInvocationIndex], groupMemoryBarrier(), gs_Keys[gs_D[gl_LocalInvocationIndex]] = _169, gs_Values[gs_D[gl_LocalInvocationIndex]] = _173, groupMemoryBarrier(), _224++)
    {
        gs_E[gl_LocalInvocationIndex] = uint(int(((gs_Keys[gl_LocalInvocationIndex] >> _224) & 1u) == 0u));
        groupMemoryBarrier();
        bool _91 = gl_LocalInvocationIndex == 0u;
        if (_91)
        {
            gs_F[gl_LocalInvocationIndex] = 0u;
        }
        else
        {
            gs_F[gl_LocalInvocationIndex] = gs_E[gl_LocalInvocationIndex - 1u];
        }
        groupMemoryBarrier();
        uint _231;
        for (uint _225 = 1u; _225 < 256u; groupMemoryBarrier(), _114 = _231, groupMemoryBarrier(), _225 = _225 << uint(1))
        {
            uint _115 = gs_F[gl_LocalInvocationIndex];
            if (gl_LocalInvocationIndex > _225)
            {
                _231 = _115 + (gs_F[gl_LocalInvocationIndex - _225]);
                continue;
            }
            else
            {
                _231 = _115;
                continue;
            }
            continue;
        }
        if (_91)
        {
            gs_TotalFalses = gs_E[255] + gs_F[255];
        }
        groupMemoryBarrier();
        if (gs_E[gl_LocalInvocationIndex] == 1u)
        {
            _226 = gs_F[gl_LocalInvocationIndex];
            continue;
        }
        else
        {
            _226 = (gl_LocalInvocationIndex - gs_F[gl_LocalInvocationIndex]) + gs_TotalFalses;
            continue;
        }
        continue;
    }
    imageStore(OutputKeys, _21, uvec4(gs_Keys[gl_LocalInvocationIndex], 0u, 0u, 0u));
    imageStore(OutputValues, _21, uvec4(gs_Values[gl_LocalInvocationIndex], 0u, 0u, 0u));
}

