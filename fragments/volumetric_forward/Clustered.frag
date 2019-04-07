
layout(early_fragment_tests) in;

SPC const bool HasDiffuse = true;
SPC const bool HasNormal = true;
SPC const bool HasAmbientOcclusion = true;
SPC const bool HasRoughness = true;
SPC const bool HasMetallic = false;

#include "Structures.glsl"
#include "Functions.glsl"

#pragma USE_RESOURCES GlobalResources
#pragma USE_RESOURCES VolumetricForward
#pragma USE_RESOURCES VolumetricForwardLights
#pragma USE_RESOURCES Material

uvec3 IdxToCoord(uint idx) {
    uvec3 result;
    result.x = idx % ClusterData.GridDim.x;
    result.y = idx % (ClusterData.GridDim.x * ClusterData.GridDim.y) / ClusterData.GridDim.x;
    result.z = idx / (ClusterData.GridDim.x * ClusterData.GridDim.y);
    return result;
}

uint CoordToIdx(uvec3 coord) {
    return coord.x + (ClusterData.GridDim.x * (coord.y + ClusterData.GridDim.y * coord.z));
}

uvec3 cluster_index_fs(in vec2 screen_pos, in float view_z) {
    uint i = uint(screen_pos.x / globals.windowSize.x);
    uint j = uint(screen_pos.y / globals.windowSize.y);
    uint k = uint(log(-view_z / globals.depthRange.x) * ClusterData.LogGridDimY);
    return uvec3(i, j, k);
}

LightingResult Lighting(in Material mtl, in uint cluster_index, vec4 eye_pos, vec4 p, vec4 n) {
    vec4 v = normalize(eye_pos - p);
    LightingResult results;

    uint lightIndex = 0;
    uint startOffset = imageLoad(PointLightGrid, int(cluster_index)).r;
    uint lightCount = imageLoad(PointLightGrid, int(cluster_index)).g;

    for (uint i = 0; i < lightCount; ++i) {
        lightIndex = imageLoad(PointLightIndexList, int(startOffset + i)).r;

        if (!PointLights.Data[lightIndex].Enabled) {
            continue;
        }

        LightingResult point_result = CalculatePointLight(PointLights.Data[lightIndex], mtl, v, p, n);
        results.Diffuse += point_result.Diffuse;
        results.Specular += point_result.Specular;
    }

    startOffset = imageLoad(SpotLightGrid, int(cluster_index)).r;
    lightCount = imageLoad(SpotLightGrid, int(cluster_index)).g;

    for (uint j = 0; j < lightCount; ++j) {

        lightIndex = imageLoad(SpotLightIndexList, int(startOffset + j)).r;

        if (!SpotLights.Data[lightIndex].Enabled) {
            continue;
        }

        LightingResult spot_result = CalculateSpotLight(SpotLights.Data[lightIndex], mtl, v, p, n);
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
    vec4 eye_pos = globals.viewPosition;

    vec4 vertexPosViewSpace = matrices.view * vec4(vPosition, 1.0f);

    const vec3 zero_vec = vec3(0.0f, 0.0f, 0.0f);

    vec4 diffuse = MaterialParameters.Data.baseColor;
    if (HasDiffuse) {
        vec4 diffuse_sample = texture(sampler2D(AlbedoMap, LinearRepeatSampler), vUV);
        if (any(notEqual(diffuse.rgb, zero_vec))) {
            diffuse *= diffuse_sample;
        }
        else {
            diffuse = diffuse_sample;
        }
    }

    vec4 ambient = vec4(MaterialParameters.Data.ambientOcclusion);
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

    vec4 emissive = MaterialParameters.Data.emissive;

    float metallic = MaterialParameters.Data.metallic;
    if (HasMetallic) {
        float metallic_sample = texture(sampler2D(MetallicMap, LinearRepeatSampler), vUV).r;
        if (metallic != 0.0f) {
            metallic *= metallic_sample;
        }
        else {
            metallic = metallic_sample;
        }
    }

    float roughness = MaterialParameters.Data.roughness;
    if (HasRoughness) {
        float roughness_sample = texture(sampler2D(RoughnessMap, LinearRepeatSampler), vUV).r;
        if (roughness != 0.0f) {
            roughness *= roughness_sample;
        }
        else {
            roughness = roughness_sample;
        }
    }

    vec4 normal;
    if (HasNormal) {
        normal = texture(sampler2D(NormalMap, LinearRepeatSampler), vUV);
    }

    uvec3 index_3d = cluster_index_fs(gl_FragCoord.xy, vertexPosViewSpace.z);
    uint index_1d = CoordToIdx(index_3d);

    LightingResult lighting_result = Lighting(MaterialParameters.Data, index_1d, eye_pos, vertexPosViewSpace, normal);
    diffuse *= vec4(lighting_result.Diffuse, 1.0f);

    backbuffer = vec4((diffuse + roughness + metallic + ambient + emissive).rgb, MaterialParameters.Data.baseColor.a);
}
