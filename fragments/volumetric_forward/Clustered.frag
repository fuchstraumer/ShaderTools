
layout(early_fragment_tests) in;

SPC const bool HasDiffuse = true;
SPC const bool HasNormal = false;
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

uvec3 ComputeClusterIndex3D(vec2 screen_pos, float view_z) {
    uint i = uint(screen_pos.x / ClusterData.ScreenSize.x);
    uint j = uint(screen_pos.y / ClusterData.ScreenSize.y);
    uint k = uint(log(-view_z / ClusterData.ViewNear) * ClusterData.LogGridDimY);
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
    
    return results;
} 

void main() {
    vec4 eye_pos = globals.viewPosition;

    vec4 vertexPosViewSpace = vPosition;

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

    //ambient *= globals.brightness;

    /*
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
    */

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

    
    uvec3 index_3d = ComputeClusterIndex3D(gl_FragCoord.xy, vPosition.z);
    uint index_1d = CoordToIdx(index_3d);
    LightingResult lighting_result = Lighting(MaterialParameters.Data, index_1d, eye_pos, vertexPosViewSpace, normal);
    vec3 lighting_result_scaled = lighting_result.Diffuse.xyz * 0.1f;
    backbuffer = vec4(diffuse.rgb * lighting_result_scaled, MaterialParameters.Data.baseColor.a);
}
