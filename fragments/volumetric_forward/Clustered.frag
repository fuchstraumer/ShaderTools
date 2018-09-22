
layout(early_fragment_tests) in;

SPC const bool HasDiffuse = false;
SPC const bool HasNormal = false;
SPC const bool HasAmbientOcclusion = false;
SPC const bool HasRoughness = false;
SPC const bool HasMetallic = false;
SPC const bool HasEmissive = false;

#include "Structures.glsl"
#include "Functions.glsl"

#pragma USE_RESOURCES GlobalResources
#pragma USE_RESOURCES VolumetricForward
#pragma USE_RESOURCES VolumetricForwardLights
#pragma USE_RESOURCES Material

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

    for (uint j = 0; j < LightCounts.NumSpotLights; ++j) {
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

    const vec3 zero_vec = vec3(0.0f, 0.0f, 0.0f);

    vec4 diffuse = MaterialParameters.baseColor;
    if (HasDiffuse) {
        vec4 diffuse_sample = texture(sampler2D(AlbedoMap, LinearRepeatSampler), vUV);
        if (any(notEqual(diffuse.rgb, zero_vec))) {
            diffuse *= diffuse_sample;
        }
        else {
            diffuse = diffuse_sample;
        }
    }

    vec4 ambient = vec4(MaterialParameters.ambientOcclusion);
    if (HasAmbientOcclusion) {
        vec4 ambient_sample = vec4(texture(sampler2D(AmbientOcclusionMap, LinearRepeatSampler), vUV).r);
        if (any(notEqual(ambient.rgb, zero_vec))) {
            ambient *= ambient_sample;
        }
        else {
            ambient = ambient_sample;
        }
    }

    ambient *= globals.brightness;

    vec4 emissive = MaterialParameters.emissive;
    if (HasEmissive) {
        vec4 emissive_sample = vec4(texture(sampler2D(EmissiveMap, LinearRepeatSampler), vUV).r);
        if (any(notEqual(emissive.rgb, zero_vec))) {
            emissive *= emissive_sample;
        }
        else {
            emissive = emissive_sample;
        }
    }

    float metallic = MaterialParameters.metallic;
    if (HasMetallic) {
        float metallic_sample = texture(sampler2D(MetallicRoughnessMap, LinearRepeatSampler), vUV).r;
        if (metallic != 0.0f) {
            metallic *= metallic_sample;
        }
        else {
            metallic = metallic_sample;
        }
    }

    float roughness = MaterialParameters.roughness;
    if (HasRoughness) {
        float roughness_sample = texture(sampler2D(MetallicRoughnessMap, LinearRepeatSampler), vUV).g;
        if (roughness != 0.0f) {
            roughness *= roughness_sample;
        }
        else {
            roughness = roughness_sample;
        }
    }

    backbuffer = vec4(1.0f, 1.0f, 1.0f, 1.0f);
}
