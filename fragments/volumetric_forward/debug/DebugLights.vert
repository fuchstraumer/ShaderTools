#include "Structures.glsl"
SPC const bool DoPointLights = true;
#pragma USE_RESOURCES GlobalResources
#pragma USE_RESOURCES Material
#pragma USE_RESOURCES VolumetricForwardLights

#pragma INTERFACE_OVERRIDE
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 0) out vec4 vPosition;
layout (location = 1) out vec3 vNormal;
layout (location = 2) out flat uint vInstanceID;
#pragma END_INTERFACE_OVERRIDE

void main() 
{
    if (DoPointLights)
    {
        vInstanceID = gl_InstanceIndex;

        PointLight light = PointLights.Data[vInstanceID];

        mat4 light_matrix = mat4(
            light.Range, 0.0f, 0.0f, light.Position.x,
            0.0f, light.Range, 0.0f, light.Position.y,
            0.0f, 0.0f, light.Range, light.Position.z,
            0.0f, 0.0f, 0.0f,        1.0f
        );

        mat4 M = light_matrix * matrices.model;
        mat4 MV = matrices.view * M;
        mat4 MVP = matrices.projection * MV;

        gl_Position = MVP * vec4(position, 1.0f);
        vPosition = MV * vec4(position, 1.0f);
        vNormal = mat3(MV) * normal;

    }
    else
    {
        vInstanceID = gl_InstanceIndex;

        SpotLight light = SpotLights.Data[vInstanceID];

        mat4 translation_matrix = mat4(
            1.0f, 0.0f, 0.0f, light.Position.x,
            0.0f, 1.0f, 0.0f, light.Position.y,
            0.0f, 0.0f, 1.0f, light.Position.z,
            0.0f, 0.0f, 0.0f, 1.0f
        );

        float _tangent = tan(light.SpotLightAngle);

        mat4 scale_matrix = mat4(
            light.Range * _tangent, 0.0f, 0.0f, 0.0f,
            0.0f, light.Range * _tangent, 0.0f, 0.0f,
            0.0f, 0.0f, light.Range * _tangent, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );

        mat4 rotation_matrix = mat4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );

        vec3 z_dir = vec3(0.0f, 0.0f, 1.0f);
        vec3 light_dir = normalize(light.Direction.xyz);
        vec3 axis_of_rotation = cross(z_dir, light_dir);
        float angle_sine = length(axis_of_rotation);

        if (angle_sine > 0.0f)
        {
            axis_of_rotation /= angle_sine;
            float angle_cosine = dot(z_dir, light_dir);
            float c2 = 1.0f - angle_cosine;

            float x = axis_of_rotation.x;
            float y = axis_of_rotation.y;
            float z = axis_of_rotation.z;

            rotation_matrix = mat4(
                x * x * c2 + angle_cosine, x * y * c2 - z * angle_sine, x * z * c2 + y * angle_sine, 0.0f,
                x * y * c2 + z * angle_sine, y * y * c2 + angle_cosine, y * z * c2 - x * angle_sine, 0.0f,
                x * z * c2 - y * angle_sine, y * z * c2 + x * angle_sine, z * z * c2 + angle_cosine, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            );

        }

        mat4 M = translation_matrix * rotation_matrix * scale_matrix * matrices.model;
        mat4 MV = matrices.view * M;
        mat4 MVP = matrices.projection * MV;

        gl_Position = MVP * vec4(position, 1.0f);
        vPosition = MV * vec4(position, 1.0f);
        vNormal = mat3(MV) * normal;

    }
}
