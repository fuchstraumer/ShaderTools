#version 450
layout(local_size_x = 512, local_size_y = 1, local_size_z = 1) in;

layout(constant_id = 0) const uint BuildStage = 0u;

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

struct AABB
{
    vec4 Min;
    vec4 Max;
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

layout(set = 2, binding = 3, std140) uniform light_counts
{
    uint NumPointLights;
    uint NumSpotLights;
    uint NumDirectionalLights;
} LightCounts;

layout(set = 2, binding = 0, std430) buffer point_lights
{
    PointLight Data[];
} PointLights;

layout(set = 0, binding = 2, std140) uniform BVHParams_1
{
    uint NumPointLights;
    uint NumSpotLights;
    uint NumDirectionalLights;
} BVHParams_1_1;

layout(set = 0, binding = 0, std430) buffer point_light_bvh
{
    AABB Data[];
} PointLightBVH;

layout(set = 2, binding = 1, std430) buffer spot_lights
{
    SpotLight Data[];
} SpotLights;

layout(set = 0, binding = 1, std430) buffer SpotLightBVH_1
{
    AABB Data[];
} SpotLightBVH_1_1;

layout(set = 1, binding = 1, r32ui) uniform readonly uimageBuffer PointLightIndices;
layout(set = 1, binding = 3, r32ui) uniform readonly uimageBuffer SpotLightIndices;

shared vec4 gs_AABBMin[512];
shared vec4 gs_AABBMax[512];

void main()
{
    switch (BuildStage)
    {
        case 0:
        {
            if (gl_GlobalInvocationID.x < LightCounts.NumPointLights)
            {
                uvec4 _525 = imageLoad(PointLightIndices, int(gl_GlobalInvocationID.x));
                uint _526 = _525.x;
                gs_AABBMin[gl_LocalInvocationIndex] = PointLights.Data[_526].PositionViewSpace - vec4(PointLights.Data[_526].Range);
                gs_AABBMax[gl_LocalInvocationIndex] = PointLights.Data[_526].PositionViewSpace + vec4(PointLights.Data[_526].Range);
            }
            else
            {
                gs_AABBMin[gl_LocalInvocationIndex] = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
                gs_AABBMax[gl_LocalInvocationIndex] = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
            }
            uint _672 = gl_LocalInvocationIndex % 32u;
            for (uint _995 = 16u; _672 < _995; )
            {
                uint _687 = gl_LocalInvocationIndex + _995;
                gs_AABBMin[gl_LocalInvocationIndex] = min(gs_AABBMin[gl_LocalInvocationIndex], gs_AABBMin[_687]);
                gs_AABBMax[gl_LocalInvocationIndex] = max(gs_AABBMax[gl_LocalInvocationIndex], gs_AABBMax[_687]);
                _995 = _995 >> uint(1);
                continue;
            }
            bool _555 = (gl_GlobalInvocationID.x % 32u) == 0u;
            if (_555)
            {
                uint _562 = gl_GlobalInvocationID.x / 32u;
                bool _564 = BVHParams_1_1.NumPointLights > 0u;
                bool _573;
                if (_564)
                {
                    uint _508[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
                    _573 = _562 < (_508[BVHParams_1_1.NumPointLights - 1u]);
                }
                else
                {
                    _573 = _564;
                }
                if (_573)
                {
                    uint _510[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
                    uint _581 = (_510[BVHParams_1_1.NumPointLights - 1u]) + _562;
                    PointLightBVH.Data[_581].Min = gs_AABBMin[gl_LocalInvocationIndex];
                    PointLightBVH.Data[_581].Max = gs_AABBMax[gl_LocalInvocationIndex];
                }
            }
            if (gl_GlobalInvocationID.x < LightCounts.NumSpotLights)
            {
                uvec4 _602 = imageLoad(SpotLightIndices, int(gl_GlobalInvocationID.x));
                uint _603 = _602.x;
                gs_AABBMin[gl_LocalInvocationIndex] = SpotLights.Data[_603].PositionViewSpace - vec4(SpotLights.Data[_603].Range);
                gs_AABBMax[gl_LocalInvocationIndex] = SpotLights.Data[_603].PositionViewSpace + vec4(SpotLights.Data[_603].Range);
            }
            else
            {
                gs_AABBMin[gl_LocalInvocationIndex] = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
                gs_AABBMax[gl_LocalInvocationIndex] = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
            }
            for (uint _998 = 16u; _672 < _998; )
            {
                uint _723 = gl_LocalInvocationIndex + _998;
                gs_AABBMin[gl_LocalInvocationIndex] = min(gs_AABBMin[gl_LocalInvocationIndex], gs_AABBMin[_723]);
                gs_AABBMax[gl_LocalInvocationIndex] = max(gs_AABBMax[gl_LocalInvocationIndex], gs_AABBMax[_723]);
                _998 = _998 >> uint(1);
                continue;
            }
            if (_555)
            {
                uint _639 = gl_GlobalInvocationID.x / 32u;
                bool _641 = BVHParams_1_1.NumSpotLights > 0u;
                bool _650;
                if (_641)
                {
                    uint _511[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
                    _650 = _639 < (_511[BVHParams_1_1.NumSpotLights - 1u]);
                }
                else
                {
                    _650 = _641;
                }
                if (_650)
                {
                    uint _512[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
                    uint _658 = (_512[BVHParams_1_1.NumSpotLights - 1u]) + _639;
                    SpotLightBVH_1_1.Data[_658].Min = gs_AABBMin[gl_LocalInvocationIndex];
                    SpotLightBVH_1_1.Data[_658].Max = gs_AABBMax[gl_LocalInvocationIndex];
                }
            }
            break;
        }
        case 1:
        {
            bool _759 = BVHParams_1_1.NumDirectionalLights < BVHParams_1_1.NumPointLights;
            bool _769;
            if (_759)
            {
                uint _741[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
                _769 = gl_GlobalInvocationID.x < _741[BVHParams_1_1.NumDirectionalLights];
            }
            else
            {
                _769 = _759;
            }
            if (_769)
            {
                uint _743[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
                uint _779 = _743[BVHParams_1_1.NumDirectionalLights] + gl_GlobalInvocationID.x;
                gs_AABBMin[_779] = PointLightBVH.Data[_779].Min;
                gs_AABBMax[_779] = PointLightBVH.Data[_779].Max;
            }
            else
            {
                gs_AABBMin[gl_LocalInvocationIndex] = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
                gs_AABBMax[gl_LocalInvocationIndex] = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
            }
            uint _924 = gl_LocalInvocationIndex % 32u;
            for (uint _993 = 16u; _924 < _993; )
            {
                uint _939 = gl_LocalInvocationIndex + _993;
                gs_AABBMin[gl_LocalInvocationIndex] = min(gs_AABBMin[gl_LocalInvocationIndex], gs_AABBMin[_939]);
                gs_AABBMax[gl_LocalInvocationIndex] = max(gs_AABBMax[gl_LocalInvocationIndex], gs_AABBMax[_939]);
                _993 = _993 >> uint(1);
                continue;
            }
            bool _798 = (gl_GlobalInvocationID.x % 32u) == 0u;
            if (_798)
            {
                uint _803 = gl_GlobalInvocationID.x / 32u;
                bool _818;
                if (_759)
                {
                    uint _745[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
                    _818 = _803 < (_745[BVHParams_1_1.NumDirectionalLights - 1u]);
                }
                else
                {
                    _818 = _759;
                }
                if (_818)
                {
                    uint _747[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
                    uint _827 = (_747[BVHParams_1_1.NumDirectionalLights - 1u]) + _803;
                    PointLightBVH.Data[_827].Min = gs_AABBMin[gl_LocalInvocationIndex];
                    PointLightBVH.Data[_827].Max = gs_AABBMax[gl_LocalInvocationIndex];
                }
            }
            bool _842 = BVHParams_1_1.NumDirectionalLights < BVHParams_1_1.NumSpotLights;
            bool _852;
            if (_842)
            {
                uint _748[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
                _852 = gl_GlobalInvocationID.x < _748[BVHParams_1_1.NumDirectionalLights];
            }
            else
            {
                _852 = _842;
            }
            if (_852)
            {
                uint _750[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
                uint _862 = _750[BVHParams_1_1.NumDirectionalLights] + gl_GlobalInvocationID.x;
                gs_AABBMin[_862] = SpotLightBVH_1_1.Data[_862].Min;
                gs_AABBMax[_862] = SpotLightBVH_1_1.Data[_862].Max;
            }
            else
            {
                gs_AABBMin[gl_LocalInvocationIndex] = vec4(3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 3.4028235931503486209399819845155e+36, 1.0);
                gs_AABBMax[gl_LocalInvocationIndex] = vec4(-3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, -3.4028235931503486209399819845155e+36, 1.0);
            }
            for (uint _994 = 16u; _924 < _994; )
            {
                uint _975 = gl_LocalInvocationIndex + _994;
                gs_AABBMin[gl_LocalInvocationIndex] = min(gs_AABBMin[gl_LocalInvocationIndex], gs_AABBMin[_975]);
                gs_AABBMax[gl_LocalInvocationIndex] = max(gs_AABBMax[gl_LocalInvocationIndex], gs_AABBMax[_975]);
                _994 = _994 >> uint(1);
                continue;
            }
            if (_798)
            {
                uint _886 = gl_GlobalInvocationID.x / 32u;
                bool _901;
                if (_842)
                {
                    uint _752[7] = uint[](1u, 32u, 1024u, 32768u, 1048576u, 33554432u, 1073741824u);
                    _901 = _886 < (_752[BVHParams_1_1.NumDirectionalLights - 1u]);
                }
                else
                {
                    _901 = _842;
                }
                if (_901)
                {
                    uint _754[7] = uint[](0u, 1u, 33u, 1057u, 33825u, 1082401u, 34636833u);
                    uint _910 = (_754[BVHParams_1_1.NumDirectionalLights - 1u]) + _886;
                    SpotLightBVH_1_1.Data[_910].Min = gs_AABBMin[gl_LocalInvocationIndex];
                    SpotLightBVH_1_1.Data[_910].Max = gs_AABBMax[gl_LocalInvocationIndex];
                }
            }
            break;
        }
    }
}

