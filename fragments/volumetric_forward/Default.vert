// also used for clustered.vert. Simple pass-through shader, really.
#include "Structures.glsl"
#pragma USE_RESOURCES GlobalResources
#pragma USE_RESOURCES VolumetricForward
void main() {
    gl_Position = matrices.modelViewProjection * vec4(position, 1.0f);
    vPosition = matrices.modelView * vec4(position, 1.0f);
    vNormal = mat3(matrices.inverseTransposeModelView) * normal;
    vTangent = mat3(matrices.inverseTransposeModelView) * tangent;
    vUV = uv;
}
