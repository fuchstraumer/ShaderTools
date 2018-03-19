
layout(early_fragment_tests) in;
#include "Structures.glsl"
#include "Functions.glsl"

#pragma USE_RESOURCES VOLUMETRIC_FORWARD
#pragma USE_RESOURCES SORT_RESOURCES
#pragma USE_RESOURCES VOLUMETRIC_FORWARD_LIGHTS
#pragma USE_RESOURCES MATERIAL_RESOURCES

LightingResult GetLighting(uint cluster_index_1d, Material material, vec4 eyePos, vec4 P, vec4 N) {
    LightingResult result;

    vec4 V = normalize(eyePos - P);

    uint startOffset = imageLoad(PointLightGrid, int(cluster_index_1d)).x;
    uint lightCount = imageLoad(PointLightGrid, int(cluster_index_1d)).y;

    for (uint i = 0; i < lightCount; ++i) {
        uint light_index = imageLoad(PointLightIndexList, int(startOffset + i));
        if (!PointLights.Data[light_index].Enabled) {
            continue;
        }

        LightingResult point_result = CalculatePointLight(PointLights.Data[light_index], materla, V, P, N);
        result.Diffuse += point_result.Diffuse;
        result.Specular += point_result.Specular;
    }

    startOffset = imageLoad(SpotLightGrid, int(cluster_index_1d)).x;
    lightCount = imageLoad(SpotLightGrid, int(cluster_index_1d)).y;

    for(uint i = 0; i < lightCount; ++i) {
        uint light_index = imageLoad(SpotLightIndexList, int(startOffset + i));
        if (!SpotLights.Data[light_index].Enabled) {
            continue;
        }

        LightingResult spotlight_result = CalculateSpotLight(SpotLights.Data[light_index], material, V, P, N);
        result.Diffuse += spotlight_result.Diffuse;
        result.Specular += spotlight_result.Specular;
    }

    for (uint i = 0; i < LightCounts.NumDirectionalLights; ++i) {
        if (!DirectionalLights.Data[i].Enabled) {
            continue;
        }

        LightingResult directional_result = CalculateDirectionalLight(DirectionalLights.Data[i], material, V, P, N);
        result.Diffuse += directional_result.Diffuse;
        result.Specular += directional_result.Specular;
    }

    return result;
}

void main() {

    vec4 ambient = Material.ambient;
    if (material.Ambient) {
        vec4 ambient_sample = texture(sampler2D(ambientMap, AnisotropicRepeatSampler), vUV);
        if (any(ambient.rgb)) {
            ambient *= ambient_sample;
        }
        else {
            ambient = ambient_sample;
        }
    }

    vec4 diffuse = Material.diffuse;
    if (TextureFlags.Diffuse) {
        vec4 diffuse_sample = texture(sampler2D(diffuseMap, AnisotropicRepeatSampler), vUV);
        if (any(diffuse_sample.rgb)) {
            diffuse *= diffuse_sample;
        }
        else {
            diffuse = diffuse_sample;
        }
    }

    float alpha = diffuse.a;
    if (TextureFlags.Opacity) {
        alpha = texture(sampler2D(opacityMap, AnisotropicRepeatSampler), vUV).r;
    }

}
