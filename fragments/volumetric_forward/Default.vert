// also used for clustered.vert. Simple pass-through shader, really.
#pragma USE_RESOURCES GlobalResources
void main() {
    gl_Position = matrices.projection * matrices.view * matrices.model * vec4(position, 1.0f);
    vPosition = vec3(matrices.model * vec4(position, 1.0f));
    vNormal = mat3(matrices.normal) * normal;
    vTangent = mat3(matrices.normal) * tangent;
    vUV = uv;
}
