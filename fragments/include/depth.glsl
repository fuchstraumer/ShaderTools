
float FCoeff() {
    return 2.0f / log2(sg_FarPlane + 1.0f);
}

float LogarithmicDepth(vec4 world_position) {
    return (log2(max(1.0e-6f, 1.0 + world_position.w)) * FCoeff() - 1.0f) * world_position.w;
}

float LogarithmicDepthInterpolant(vec4 world_position) {
    return log2(1.0f + world_position.w) * FCoeff() * 0.50f;
}

// Take logarithmic depth value and re-linearize it
float LinearizeLogDepth(float log_depth) {
    const float a = sg_FarPlane / (sg_FarPlane - sg_NearPlane);
    const float b = sg_FarPlane * sg_NearPlane / (sg_NearPlane - sg_FarPlane);
    return (a + b) / log_depth;
}

float InvertLogarithmicDepth(float log_depth) {
    return LinearizeDepth(pow(sg_FarPlane + 1.0, log_depth) - 1.0f);
}

float LinearDepth(float depth) {
    const float z = depth * 2.0f - 1.0f;
    return (2.0f * sg_NearPlane * sg_FarPlane) / (sg_FarPlane + sg_NearPlane - z * (sg_FarPlane - sg_NearPlane));
}