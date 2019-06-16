#include "Structures.glsl"
SPC const bool DoPointLights = true;
#pragma USE_RESOURCES GlobalResources
#pragma USE_RESOURCES VolumetricForwardLights

#pragma INTERFACE_OVERRIDE
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 tangent;
layout (location = 3) in vec2 uv;
layout (location = 0) out flat uint vInstanceID;
#pragma END_INTERFACE_OVERRIDE

void main() 
{
    [[flatten]]
    if (DoPointLights)
    {
        vInstanceID = gl_InstanceIndex;
        PointLight light = PointLights.Data[vInstanceID];
        vec4 transformed_position = vec4(light.Range * (light.Position.xyz + position), 1.0f);
        gl_Position = matrices.projection * matrices.view * transformed_position;
    }
    else
    {
        vInstanceID = gl_InstanceIndex;
        SpotLight light = SpotLights.Data[vInstanceID];
        vec4 transformed_position = vec4(light.Range * (light.Position.xyz + position), 1.0f);
        gl_Position = matrices.projection * matrices.view * transformed_position;

    }
}
