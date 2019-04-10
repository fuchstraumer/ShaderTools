#pragma INTERFACE_OVERRIDE
layout (location = 0) in vec4 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in flat uint vInstanceID;
layout (location = 0) out vec4 backbuffer;
#pragma END_INTERFACE_OVERRIDE

#include "Structures.glsl"

#pragma USE_RESOURCES GlobalResources
#pragma USE_RESOURCES VolumetricForwardLights

// Update before baking pipeline. Used to set rendering mode.
SPC const bool DoPointLights = true;

void main()
{
    [[flatten]]
    if (DoPointLights)
    {
        PointLight light = PointLights.Data[vInstanceID];
        vec4 N = vec4(normalize(vNormal), 0.0f);
        backbuffer = vec4((light.Color * clamp(N.z, 0.0f, 1.0f)), 0.40f);
    }
    else
    {
        SpotLight light = SpotLights.Data[vInstanceID];
        vec4 N = vec4(normalize(vNormal), 0.0f);
        backbuffer = vec4((light.Color * clamp(N.z, 0.0f, 1.0f)),  0.40f);
    }

}