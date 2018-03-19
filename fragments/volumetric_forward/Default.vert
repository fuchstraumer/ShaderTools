// also used for clustered.vert. Simple pass-through shader, really.
void main() {
    gl_Position = matrices.Projection * matrices.View * matrices.Model * vec4(position, 1.0f);
    vPos = vec3(matrices.Model * vec4(position, 1.0f));
    vNormal = mat3(matrices.Normal) * normal;
    vTangent = mat3(matrices.Normal) * tangent;
    vUV = uv;
}
