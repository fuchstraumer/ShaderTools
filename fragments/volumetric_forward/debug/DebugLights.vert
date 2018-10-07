#include "Structures.glsl"
#include "Functions.glsl"

#pragma USE_RESOURCES GLOBAL_RESOURCES
#pragma USE_RESOURCES MATERIAL
#pragma USE_RESOURCES VOLUMETRIC_FORWARD_LIGHTS

#pragma INTERFACE_OVERRIDE
in vec3 position;
in vec3 normal;
in vec3 tangent;
in vec2 uv;
out vec3 vNormal;

#pragma INTERFACE_OVERRIDE_END

// Update before baking pipeline. Used to set rendering mode.
SPC const uint NUM_POINT_LIGHTS = 1024u;

void main() {
    [[flatten]]
    if (gl_InstanceID < NUM_POINT_LIGHTS) {
        PointLight pl = PointLights.Data[gl_InstanceID];

        mat4 light_matrix = mat4(
            pl.Range, 0.0f, 0.0f, pl.Position.x,
            0.0f, pl.Range, 0.0f, pl.Position.y,
            0.0f, 0.0f, pl.Range, pl.Position.z,
            0.0f, 0.0f, 0.0f,     1.0f
        );

        mat4 m = light_matrix * matrices.model;
        mat4 mv = matrices.view * m;
        mat4 mvp = matrices.projection * mv;

        vPosition = mvp * position;
        gl_Position = vPosition;
        vNormal = mat3(MV) * normal;
        vTangent = mat3(MV) * tangent;
        vUV = uv;

    }
    else {

    }
}
