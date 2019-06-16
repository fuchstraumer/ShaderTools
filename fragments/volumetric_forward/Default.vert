// also used for clustered.vert. Simple pass-through shader, really.
#include "Structures.glsl"
#pragma USE_RESOURCES GlobalResources

void main() {
    gl_Position = matrices.modelViewProjection * vec4(position, 1.0f);
    vPosition = matrices.modelView * vec4(position, 1.0f);
    vNormal = mat3(inverse(transpose(matrices.model))) * normal;
    vTangent = mat3(inverse(transpose(matrices.model))) * tangent;
    vUV = uv;
}
