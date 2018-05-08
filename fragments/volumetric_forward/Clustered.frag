
layout(early_fragment_tests) in;
#include "MaterialSpecialization.glsl"
#include "Structures.glsl"
#include "Functions.glsl"

#pragma USE_RESOURCES GlobalResources
#pragma USE_RESOURCES VolumetricForward
#pragma USE_RESOURCES VolumetricForwardLights

LightingResult Lighting(in Material mtl, vec4 eye_pos, vec4 p, vec4 n) {
    vec4 v = normalize(eye_pos - p);
    LightingResult results;

    for (uint i = 0; i < LightCounts.NumPointLights; ++i) {
        if (!PointLights.Data[i].Enabled) {
            continue;
        }

        LightingResult point_result = CalculatePointLight(PointLights.Data[i], mtl, v, p, n);
        results.Diffuse += point_result.Diffuse;
        results.Specular += point_result.Specular;
    }

    for (uint j = 0; j < LightCounts.NumSpotLights; ++i) {
        if (!SpotLights.Data[j].Enabled) {
            continue;
        }

        LightingResult spot_result = CalculateSpotLight(SpotLights.Data[j], mtl, v, p, n);
        results.Diffuse += spot_result.Diffuse;
        results.Specular += spot_result.Specular;
    }

    for (uint k = 0; k < LightCounts.NumDirectionalLights; ++k) {
        if (!DirectionalLights.Data[k].Enabled) {
            continue;
        }

        LightingResult dir_result = CalculateDirectionalLight(DirectionalLights.Data[k], mtl, v, p, n);
        results.Diffuse += dir_result.Diffuse;
        results.Specular += dir_result.Specular;
    }

    return results;
} 

void main() {
    vec4 eye_pos = vec4(0.0f, 0.0f, 0.0f, 1.0f);

    Material mtl = MaterialParameters.Data;

    vec4 diffuse = mtl.diffuse;
    if (HasAmbientTexture) {
        vec4 diffuse_sample = texture(sampler2D(AmbientMap, LinearRepeatSampler), vUV);
        if (any(diffuse.rgb)) {
            diffuse *= diffuse_sample;
        }
        else {
            diffuse = diffuse_sample;
        }
    }

    float alpha = mtl.alpha;
    if (HasAlphaTexture) {
        alpha = texture(sampler2D(AlphaMap, LinearRepeatSampler), vUV).r;
    }

    vec4 ambient = mtl.ambient;
    if (HasAmbientTexture) {
        vec4 ambient_sample = texture(sampler2D(AmbientMap, LinearRepeatSampler), vUV).r;
        if (any(ambient.rgb)) {
            ambient *= ambient_sample;
        }
        else {
            ambient = ambient_sample;
        }
    }

    ambient *= mtl.globalAmbient;

    vec4 emissive = mtl.emissive;
    if (HasEmissiveTexture) {
        vec4 emissive_sample = texture(sampler2D(EmissiveMap, LinearRepeatSampler), vUV).r;
        if (any(emissive.rgb)) {
            emissive *= emissive_sample;
        }
        else {
            emissive = emissive_sample;
        }
    }

    float metallic = mtl.metallic;
    if (HasMetallicTexture) {
        float metallic_sample = texture(sampler2D(MetallicMap, LinearRepeatSampler), vUV).r;
        if (metallic != 0.0f) {
            metallic *= metallic_sample;
        }
        else {
            metallic = metallic_sample;
        }
    }

    float roughness = mtl.roughness;
    if (HasRoughnessTexture) {
        float roughness_sample = texture(sampler2D(RoughnessMap, LinearRepeatSampler), vUV).r;
        if (roughness != 0.0f) {
            roughness *= roughness_sample;
        }
        else {
            roughness = roughness_sample;
        }
    }

    backbuffer = vec4(1.0f, 1.0f, 1.0f, 1.0f);
}
