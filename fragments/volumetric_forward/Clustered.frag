
layout(early_fragment_tests) in;

#include "Structures.glsl"
#include "Functions.glsl"

#pragma USE_RESOURCES GlobalResources
#pragma USE_RESOURCES VolumetricForward
#pragma USE_RESOURCES VolumetricForwardLights
#pragma USE_RESOURCES Material

layout (push_constant) uniform push_consts {
    bool hasAlbedoMap;
    bool hasAlphaMap;
    bool hasSpecularMap;
    bool hasBumpMap;
    bool hasDisplacementMap;
    bool hasNormalMap;
    bool hasAmbientOcclusionMap;
    bool hasMetallicMap;
    bool hasRoughnessMap;
    bool hasEmissiveMap;
    bool materialLoading;
};

uvec3 IdxToCoord(uint idx)
{
    uvec3 result;
    result.x = idx % ClusterData.GridDim.x;
    result.y = idx % (ClusterData.GridDim.x * ClusterData.GridDim.y) / ClusterData.GridDim.x;
    result.z = idx / (ClusterData.GridDim.x * ClusterData.GridDim.y);
    return result;
}

uint CoordToIdx(uvec3 coord)
{
    return coord.x + (ClusterData.GridDim.x * (coord.y + ClusterData.GridDim.y * coord.z));
}

uvec3 ComputeClusterIndex3D(vec2 screen_pos, float view_z)
{
    uint i = uint(screen_pos.x / ClusterData.ScreenSize.x);
    uint j = uint(screen_pos.y / ClusterData.ScreenSize.y);
    uint k = uint(log(-view_z / ClusterData.ViewNear) * ClusterData.LogGridDimY);
    return uvec3(i, j, k);
}

vec3 getNormalFromMap(in vec3 sampled_normal, in vec3 position, in vec2 uv, in vec3 vert_normal)
{
    vec3 Q1 = dFdx(vPosition).xyz;
    vec3 Q2 = dFdy(vPosition).xyz;
    vec2 st1 = dFdx(uv);
    vec2 st2 = dFdy(uv);

    vec3 N = normalize(vert_normal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    
    return normalize(TBN * sampled_normal);
}

LightingResult calculateLight(in const vec3 lightColor, in const float lightRange, in const float lightIntensity, in const vec3 lightPos, in const vec3 V, 
    in const vec3 P, in const vec3 F0, in const float metallic, in const float roughness, in const vec3 albedo)
{
    LightingResult result;
    vec3 vertexNormal = normalize(vNormal).xyz;
    vec3 N;
    if (hasNormalMap)
    {
        vec3 sampledNormal = ExpandNormal(texture(sampler2D(NormalMap, LinearRepeatSampler), vUV).xyz);
        N = getNormalFromMap(sampledNormal, P, vUV, vertexNormal);
    }
    else
    {
        N = vertexNormal;
    }

    vec3 L = lightPos - P;
    float dist = length(L);

    float attenuation = Attenuation(lightRange, dist);
    L = normalize(L);

    vec3 H = V + L;
    vec3 radiance = lightColor * attenuation * lightIntensity;

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0f), F0);
    float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
    result.Specular = (NDF * G * F) / denominator;

    vec3 kD = vec3(1.0f) - F;
    kD *= 1.0f - metallic;
    float NdotL = max(dot(N, L), 0.0f);

    result.Diffuse = (kD * albedo / 3.141592f + result.Specular) * radiance * NdotL;
    return result;
}

LightingResult CalculatePointLight(in const PointLight light, in const vec3 V, in const vec3 P, in const vec3 F0, in const float metallic, 
    in const float roughness, in const vec3 albedo) {
    return calculateLight(light.Color.rgb, light.Range, light.Intensity, light.PositionViewSpace.xyz, V, P, F0, metallic, roughness, albedo);
}

LightingResult CalculateDirectionalLight(in const DirectionalLight light, vec4 V, vec4 P, vec4 N) {
    LightingResult result;
    result.Diffuse = vec3(0.0f);
    result.Specular = vec3(0.0f);
    return result;
}

LightingResult CalculateSpotLight(in const SpotLight light, in const vec3 V, in const vec3 P, in const vec3 F0, in const float metallic, 
    in const float roughness, in const vec3 albedo) {
    vec3 L = normalize(light.PositionViewSpace.xyz - P.xyz);
    float spot_intensity = GetSpotLightCone(light, L) * light.Intensity;
    return calculateLight(light.Color.rgb, light.Range, spot_intensity, light.PositionViewSpace.xyz, V, P, F0, metallic, roughness, albedo);
}

LightingResult Lighting(in uint cluster_index, vec4 eye_pos, vec4 p, vec4 n, in float roughness, in vec3 albedo) {
    vec4 v = normalize(eye_pos - p);
    LightingResult results;
    results.Diffuse  = vec3(0.0f, 0.0f, 0.0f);

    uint lightIndex = 0;
    uint startOffset = imageLoad(PointLightGrid, int(cluster_index)).r;
    uint lightCount = imageLoad(PointLightGrid, int(cluster_index)).g;

    const vec3 F0 = vec3(0.04f);

    for (uint i = 0; i < lightCount; ++i) {
        lightIndex = imageLoad(PointLightIndexList, int(startOffset + i)).r;

        PointLight pointLight = PointLights.Data[lightIndex];

        if (!pointLight.Enabled)
        {
            continue;
        }

        LightingResult plResults = CalculatePointLight(pointLight, v.xyz, p.xyz, F0, 0.0001f, roughness, albedo);
        results.Diffuse += plResults.Diffuse;
        results.Specular += plResults.Specular;
    }

    startOffset = imageLoad(SpotLightGrid, int(cluster_index)).r;
    lightCount = imageLoad(SpotLightGrid, int(cluster_index)).g;

    for (uint i = 0; i < lightCount; ++i)
    {
        lightIndex = imageLoad(SpotLightIndexList, int(startOffset + i)).r;

        SpotLight light = SpotLights.Data[lightIndex];

        if (!light.Enabled)
        {
            continue;
        }
        
        LightingResult spResults = CalculateSpotLight(light, v.xyz, p.xyz, F0, 0.0001f, roughness, albedo);
        results.Diffuse += spResults.Diffuse;
        results.Specular += spResults.Specular;
    }

    /*
    for (uint i = 0; i < LightCounts.NumDirectionalLights; ++i)
    {
        if (!DirectionalLights.Data[i].Enabled)
        {
            continue;
        }

        LightingResult dirResult = CalculateDirectionalLight(DirectionalLights.Data[i], mtl, v, p, n);
        results.Diffuse += dirResult.Diffuse;
        results.Specular += dirResult.Specular;
    }
    */
    
    return results;
} 

void main() {
    vec4 eye_pos = globals.viewPosition;

    vec4 vertexPosViewSpace = vPosition;

    const vec3 zero_vec = vec3(0.0f, 0.0f, 0.0f);

    vec4 diffuse = vec4(MaterialParameters.diffuse, 1.0f);
    if (hasAlbedoMap)
    {
        vec4 diffuse_sample = texture(sampler2D(AlbedoMap, LinearRepeatSampler), vUV);
        if (any(notEqual(diffuse.rgb, zero_vec)))
        {
            diffuse *= diffuse_sample;
        }
        else
        {
            diffuse = diffuse_sample;
        }
    }

    float roughness = MaterialParameters.roughness;
    if (hasRoughnessMap)
    {
        float roughness_sample = texture(sampler2D(RoughnessMap, LinearRepeatSampler), vUV).r;
        if (roughness != 0.0f)
        {
            roughness *= roughness_sample;
        }
        else
        {
            roughness = roughness_sample;
        }
    }

    uvec3 index_3d = ComputeClusterIndex3D(gl_FragCoord.xy, vPosition.z);
    uint index_1d = CoordToIdx(index_3d);
    LightingResult lit = Lighting(index_1d, eye_pos, vertexPosViewSpace, vec4(0.0f), roughness, diffuse.rgb);

    if (materialLoading)
    {
        // material loading. set base color as light grey then add lighting contribution.
        if (any(notEqual(MaterialParameters.diffuse.rgb, zero_vec)))
        {
            backbuffer.rgb = MaterialParameters.diffuse.rgb;
            backbuffer.a = 1.0f;
        }
        backbuffer.rgb += lit.Diffuse.rgb;
        return;
    }

    vec4 ambient = vec4(MaterialParameters.ambient, 1.0f);
    if (hasAmbientOcclusionMap)
    {
        vec4 ambient_sample = vec4(texture(sampler2D(AmbientOcclusionMap, LinearRepeatSampler), vUV).r);
        if (any(notEqual(ambient.rgb, zero_vec)))
        {
            ambient *= ambient_sample;
        }
        else
        {
            ambient = ambient_sample;
        }
    }

    //ambient *= globals.brightness;

    vec3 emissive = MaterialParameters.emissive;
    if (hasEmissiveMap)
    {
        float emissive_sample = texture(sampler2D(EmissiveMap, LinearRepeatSampler), vUV).r;
        if (any(notEqual(emissive, zero_vec)))
        {
            emissive *= emissive_sample;
        }
        else
        {
            emissive = vec3(emissive_sample);
        }
    }

    float metallic = MaterialParameters.metallic;
    if (hasMetallicMap)
    {
        float metallic_sample = texture(sampler2D(MetallicMap, LinearRepeatSampler), vUV).r;
        if (metallic != 0.0f)
        {
            metallic *= metallic_sample;
        }
        else
        {
            metallic = metallic_sample;
        }
    }

    vec3 out_color = (diffuse.rgb * ambient.rgb * vec3(0.1f)) + lit.Diffuse.rgb;
    out_color = out_color / (out_color + vec3(1.0f));
    out_color = pow(out_color, vec3(1.0f / 2.20f));

    backbuffer = vec4(out_color, 1.0f);
}
