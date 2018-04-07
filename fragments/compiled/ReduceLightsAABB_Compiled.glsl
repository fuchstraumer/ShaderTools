#version 450
layout(local_size_x = 512, local_size_y = 1, local_size_z = 1) in;

layout(constant_id = 0) const uint NumThreads = 512u;
layout(constant_id = 1) const uint ReductionType = 0u;

struct AABB
{
    vec4 Min;
    vec4 Max;
};

struct PointLight
{
    vec4 Position;
    vec4 PositionViewSpace;
    vec3 Color;
    float Range;
    float Intensity;
    uint Enabled;
    vec2 Padding;
};

struct SpotLight
{
    vec4 Position;
    vec4 PositionViewSpace;
    vec4 Direction;
    vec4 DirectionViewSpace;
    vec3 Color;
    float SpotLightAngle;
    float Range;
    float Intensity;
    uint Enabled;
    float Padding;
};

layout(set = 1, binding = 4, std430) buffer global_aabb
{
    AABB Data[];
} LightAABBs;

layout(set = 0, binding = 3, std140) uniform light_counts
{
    uint NumPointLights;
    uint NumSpotLights;
    uint NumDirectionalLights;
} LightCounts;

layout(set = 0, binding = 0, std430) buffer point_lights
{
    PointLight Data[];
} PointLights;

layout(set = 1, binding = 5, std140) uniform dispatch_params
{
    uvec3 NumThreadGroups;
    uvec3 NumThreads;
} DispatchParams;

layout(set = 0, binding = 1, std430) buffer spot_lights
{
    SpotLight Data[];
} SpotLights;

layout(set = 1, binding = 6, std140) uniform reduction_params
{
    uint NumElements;
} ReductionParams;

shared vec4 gs_AABBMin[NumThreads];
shared vec4 gs_AABBMax[NumThreads];

void main()
{
    switch (ReductionType)
    {
        case 0:
        {
            vec4 _625;
            vec4 _627;
            _627 = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
            _625 = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
            for (uint _622 = gl_GlobalInvocationID.x; _622 < LightCounts.NumPointLights; )
            {
                _627 = max(_627, PointLights.Data[_622].PositionViewSpace + vec4(PointLights.Data[_622].Range));
                _625 = min(_625, PointLights.Data[_622].PositionViewSpace - vec4(PointLights.Data[_622].Range));
                _622 += (NumThreads * DispatchParams.NumThreadGroups.x);
                continue;
            }
            vec4 _624;
            vec4 _626;
            _626 = _627;
            _624 = _625;
            for (uint _623 = gl_GlobalInvocationID.x; _623 < LightCounts.NumSpotLights; )
            {
                _626 = min(_626, SpotLights.Data[_623].PositionViewSpace + vec4(SpotLights.Data[_623].Range));
                _624 = min(_624, SpotLights.Data[_623].PositionViewSpace - vec4(SpotLights.Data[_623].Range));
                _623 += (NumThreads * DispatchParams.NumThreadGroups.x);
                continue;
            }
            gs_AABBMin[gl_LocalInvocationIndex] = _624;
            gs_AABBMax[gl_LocalInvocationIndex] = _626;
            groupMemoryBarrier();
            uint _628;
            _628 = (NumThreads >> uint(1));
            for (; _628 > 32u; groupMemoryBarrier(), _628 = _628 >> uint(1))
            {
                if (gl_LocalInvocationIndex < _628)
                {
                    uint _413 = gl_LocalInvocationIndex + _628;
                    gs_AABBMin[gl_LocalInvocationIndex] = min(gs_AABBMin[gl_LocalInvocationIndex], gs_AABBMin[_413]);
                    gs_AABBMax[gl_LocalInvocationIndex] = max(gs_AABBMax[gl_LocalInvocationIndex], gs_AABBMax[_413]);
                    continue;
                }
                else
                {
                    continue;
                }
                continue;
            }
            if (gl_LocalInvocationIndex < 32u)
            {
                for (uint _629 = _628; _629 > 0u; _629 = _629 >> uint(1))
                {
                    if (NumThreads >= (_629 << uint(1)))
                    {
                        uint _453 = gl_LocalInvocationIndex + _629;
                        gs_AABBMin[gl_LocalInvocationIndex] = min(gs_AABBMin[gl_LocalInvocationIndex], gs_AABBMin[_453]);
                        gs_AABBMax[gl_LocalInvocationIndex] = max(gs_AABBMax[gl_LocalInvocationIndex], gs_AABBMax[_453]);
                        continue;
                    }
                    else
                    {
                        continue;
                    }
                    continue;
                }
                if (gl_LocalInvocationIndex == 0u)
                {
                    LightAABBs.Data[gl_WorkGroupID.x].Min = gs_AABBMin[gl_LocalInvocationIndex];
                    LightAABBs.Data[gl_WorkGroupID.x].Max = gs_AABBMax[gl_LocalInvocationIndex];
                }
            }
            break;
        }
        case 1:
        {
            vec4 _616;
            vec4 _617;
            _617 = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
            _616 = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
            for (uint _615 = gl_LocalInvocationIndex; _615 < ReductionParams.NumElements; )
            {
                _617 = max(_617, LightAABBs.Data[_615].Max);
                _616 = min(_616, LightAABBs.Data[_615].Min);
                _615 += (NumThreads * DispatchParams.NumThreadGroups.x);
                continue;
            }
            gs_AABBMin[gl_LocalInvocationIndex] = _616;
            gs_AABBMax[gl_LocalInvocationIndex] = _617;
            groupMemoryBarrier();
            uint _618;
            _618 = (NumThreads >> uint(1));
            for (; _618 > 32u; groupMemoryBarrier(), _618 = _618 >> uint(1))
            {
                if (gl_LocalInvocationIndex < _618)
                {
                    uint _541 = gl_LocalInvocationIndex + _618;
                    gs_AABBMin[gl_LocalInvocationIndex] = min(gs_AABBMin[gl_LocalInvocationIndex], gs_AABBMin[_541]);
                    gs_AABBMax[gl_LocalInvocationIndex] = max(gs_AABBMax[gl_LocalInvocationIndex], gs_AABBMax[_541]);
                    continue;
                }
                else
                {
                    continue;
                }
                continue;
            }
            if (gl_LocalInvocationIndex < 32u)
            {
                for (uint _619 = _618; _619 > 0u; _619 = _619 >> uint(1))
                {
                    if (NumThreads >= (_619 << uint(1)))
                    {
                        uint _581 = gl_LocalInvocationIndex + _619;
                        gs_AABBMin[gl_LocalInvocationIndex] = min(gs_AABBMin[gl_LocalInvocationIndex], gs_AABBMin[_581]);
                        gs_AABBMax[gl_LocalInvocationIndex] = max(gs_AABBMax[gl_LocalInvocationIndex], gs_AABBMax[_581]);
                        continue;
                    }
                    else
                    {
                        continue;
                    }
                    continue;
                }
                if (gl_LocalInvocationIndex == 0u)
                {
                    LightAABBs.Data[gl_WorkGroupID.x].Min = gs_AABBMin[gl_LocalInvocationIndex];
                    LightAABBs.Data[gl_WorkGroupID.x].Max = gs_AABBMax[gl_LocalInvocationIndex];
                }
            }
            break;
        }
    }
}

