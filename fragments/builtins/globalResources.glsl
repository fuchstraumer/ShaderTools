#pragma BEGIN_RESOURCES GLOBAL_RESOURCES

UNIFORM_BUFFER mvp_matrix_ubo {
    mat4 Model;
    mat4 View;
    mat4 Projection;
    mat4 Normal;
} matrices;

UNIFORM_BUFFER misc_data_ubo {
    vec4 viewPosition;
    vec2 mousePosition;
    vec2 windowSize;
    vec2 depthRange;
    uint frame;
} globals;

UNIFORM_BUFFER lighting_data_ubo {
    float Exposure;
    float Gamma;
    uint NumLights;
} lighting_data;

SAMPLER_2D LinearRepeatSampler;
SAMPLER_2D LinearClampSampler;
SAMPLER_2D AnisotropicRepeatSampler;
SAMPLER_2D AnisotropicClampSampler;

#pragma END_RESOURCES GLOBAL_RESOURCES

