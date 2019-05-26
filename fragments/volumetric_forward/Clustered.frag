
layout(early_fragment_tests) in;

// can adjust range based on hardware
SPC const uint minParallaxLayersUINT = 8u;
SPC const uint maxParallaxLayersUINT = 32u;

#include "Structures.glsl"
#include "Functions.glsl"

#pragma USE_RESOURCES GlobalResources
#pragma USE_RESOURCES VolumetricForward
#pragma USE_RESOURCES VolumetricForwardLights
#pragma USE_RESOURCES Material

layout (push_constant) uniform push_consts {
    layout (offset = 0) bool hasAlbedoMap;
    layout (offset = 4) bool hasAlphaMap;
    layout (offset = 8) bool hasSpecularMap;
    layout (offset =12) bool hasBumpMap;
    layout (offset =16) bool hasDisplacementMap;
    layout (offset =20) bool hasNormalMap;
    layout (offset =24) bool hasAmbientOcclusionMap;
    layout (offset =28) bool hasMetallicMap;
    layout (offset =32) bool hasRoughnessMap;
    layout (offset =36) bool hasEmissiveMap;
    layout (offset =40) bool materialLoading;
};

struct lightingInput
{
    vec3 lightColor;
    float lightRange;
    vec3 lightPos;
    float lightIntensity;
    vec3 viewDir;
    vec3 viewPos;
    vec2 uv;
    vec3 diffuseColor;
    vec3 specularColor;
    float roughness;
    float metallic;
    vec3 worldPos;
    float pad0;
    vec3 fragPos;
    float pad1;
    mat3 TBN;
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

mat3 getTBN()
{   
    vec3 N = normalize(vNormal.xyz);
    vec3 T = normalize(vTangent.xyz);
    vec3 B = normalize(cross(N, T));
    return mat3(T, B, N);
}

vec3 doNormalMapping(in vec2 input_uv, in mat3 TBN)
{
    vec3 sampled_normal = ExpandNormal(texture(sampler2D(NormalMap, LinearRepeatSampler), input_uv).xyz);
    return normalize(TBN * sampled_normal);
}

vec3 doBumpMapping(in vec2 uv, in mat3 TBN)
{
    float height00 = texture(sampler2D(BumpMap, LinearRepeatSampler), uv).r;
    float height10 = dFdxFine(height00);
    float height01 = dFdyFine(height10);

    vec3 p00 = vec3(0.0f, 0.0f, height00);
    vec3 p10 = vec3(0.0f, 0.0f, height01);
    vec3 p01 = vec3(0.0f, 0.0f, height10);

    vec3 normal = cross(normalize(p10 - p00), normalize(p01 - p00));

    return normalize(TBN * normal);
}

vec2 doParallaxMapping(in vec2 uv, in vec3 viewDir)
{
    const float minLayers = float(minParallaxLayersUINT);
    const float maxLayers = float(maxParallaxLayersUINT);
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0f, 0.0f, 1.0f), viewDir)));

    float layerDepth = 1.0f / numLayers;
    float currDepth = 0.0f;

    vec2 P = viewDir.xy / viewDir.z * MaterialParameters.height_scale;
    vec2 deltaP = P / numLayers;

    vec2 currentUV = uv;
    float currentDepthMapVal = texture(sampler2D(DisplacementMap, LinearRepeatSampler), currentUV).r;

    while (currDepth < currentDepthMapVal)
    {
        currentUV -= deltaP;
        currentDepthMapVal = texture(sampler2D(DisplacementMap, LinearRepeatSampler), currentUV).r;
        currDepth += layerDepth;
    }

    vec2 prevCoords = currentUV + P;
    float afterDepth = currentDepthMapVal - currDepth;
    float beforeDepth = texture(sampler2D(DisplacementMap, LinearRepeatSampler), prevCoords).r - currDepth + layerDepth;
    float weight = afterDepth / (afterDepth - beforeDepth);
    
    return prevCoords * weight + currentUV * (1.0f - weight);
}

void calculateLight(in const lightingInput lighting_input, inout LightingResult result)
{
    vec3 N = vec3(0.0f);

    if (hasNormalMap)
    {
        N = doNormalMapping(lighting_input.uv, lighting_input.TBN);
    }
    else if (hasBumpMap)
    {
        N = doBumpMapping(lighting_input.uv, lighting_input.TBN);
    }
    else
    {
        N = normalize(vNormal).xyz;
    }

    vec3 L = lighting_input.lightPos - lighting_input.viewPos;
    float dist = length(L);

    float attenuation = Attenuation(lighting_input.lightRange, dist);
    L = normalize(L);

    vec3 H = lighting_input.viewDir + L;
    vec3 radiance = lighting_input.lightColor * attenuation * lighting_input.lightIntensity;
    vec3 F0 = vec3(0.03f);
    float NDF = DistributionGGX(N, H, lighting_input.roughness);
    float G = GeometrySmith(N, lighting_input.viewDir, L, lighting_input.roughness);
    vec3 F = fresnelSchlick(max(dot(H, lighting_input.viewDir), 0.0f), F0);
    float denominator = 4.0f * max(dot(N, lighting_input.viewDir), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;

    result.Specular += (NDF * G * F) / denominator;

    vec3 kD = vec3(1.0f) - F;
    kD *= 1.0f - lighting_input.metallic;
    float NdotL = max(dot(N, L), 0.0f);

    result.Diffuse += (kD * lighting_input.diffuseColor / 3.141592f + result.Specular) * radiance * NdotL;

}

LightingResult CalculateDirectionalLight(in const DirectionalLight light, vec4 V, vec4 P, vec4 N) {
    LightingResult result;
    result.Diffuse = vec3(0.0f);
    result.Specular = vec3(0.0f);
    return result;
}

LightingResult Lighting(in uint cluster_index, in lightingInput lighting_input)
{
    vec3 v = normalize(lighting_input.viewPos.xyz - lighting_input.fragPos.xyz);
    lighting_input.viewDir = v;
    LightingResult results;

    results.Diffuse  = vec3(0.0f, 0.0f, 0.0f);
    results.Specular = vec3(0.0f, 0.0f, 0.0f);

    uint lightIndex = 0;
    uint startOffset = imageLoad(PointLightGrid, int(cluster_index)).r;
    uint lightCount = imageLoad(PointLightGrid, int(cluster_index)).g;

    const vec3 F0 = vec3(0.04f);

    for (uint i = 0; i < lightCount; ++i)
    {
        lightIndex = imageLoad(PointLightIndexList, int(startOffset + i)).r;

        PointLight pointLight = PointLights.Data[lightIndex];

        if (!pointLight.Enabled)
        {
            continue;
        }

        lighting_input.lightColor = pointLight.Color.xyz;
        lighting_input.lightRange = pointLight.Range;
        lighting_input.lightPos = pointLight.PositionViewSpace.xyz;

        calculateLight(lighting_input, results);
    }

    startOffset = imageLoad(SpotLightGrid, int(cluster_index)).r;
    lightCount = imageLoad(SpotLightGrid, int(cluster_index)).g;

    /*for (uint i = 0; i < lightCount; ++i)
    {
        lightIndex = imageLoad(SpotLightIndexList, int(startOffset + i)).r;

        SpotLight light = SpotLights.Data[lightIndex];

        if (!light.Enabled)
        {
            continue;
        }

        lighting_input.lightColor = light.Color.rgb;
        lighting_input.lightRange = light.Range;
        lighting_input.lightPos = light.PositionViewSpace.xyz;
        vec3 L = normalize(lighting_input.lightPos - lighting_input.viewPos.xyz);
        lighting_input.lightIntensity = light.Intensity * GetSpotLightCone(light, L)
        calculateLight(lighting_input, results);

    }*/
    
    return results;
}

void main() {

    vec4 eye_pos = globals.viewPosition;
    uvec3 index_3d = ComputeClusterIndex3D(gl_FragCoord.xy, vPosition.z);
    uint index_1d = CoordToIdx(index_3d);

    // simple world brightness before we get IBL/GI/etc
    vec3 baseDiffuse = vec3(globals.brightness);
    vec4 vertexPosViewSpace = vPosition;
    const mat3 TBN = getTBN();
    const vec3 zero_vec = vec3(0.0f, 0.0f, 0.0f);

    // init lighting input data we have so far for early-out branch
    lightingInput lighting_input;
    lighting_input.viewPos = eye_pos.xyz;
    lighting_input.TBN = TBN;
    lighting_input.worldPos = vec3(0.0f);
    lighting_input.fragPos = vertexPosViewSpace.xyz;

    if (materialLoading)
    {
        LightingResult lit = Lighting(index_1d, lighting_input);
        // material loading. set base color as light grey then add lighting contribution.
        if (any(notEqual(MaterialParameters.diffuse.rgb, zero_vec)))
        {
            backbuffer.rgb = MaterialParameters.diffuse.rgb;
            backbuffer.a = 1.0f;
        }
        else
        {
            backbuffer = vec4(0.4f, 0.4f, 0.41f, 1.0f);
        }
        backbuffer.rgb += lit.Diffuse.rgb;
        return;
    }

    vec2 fragmentUV = vUV;
    if (hasDisplacementMap)
    {
        vec3 tViewPos = TBN * globals.viewPosition.xyz;
        vec3 tFragPos = TBN * gl_FragCoord.xyz;
        vec3 tViewDir = normalize(tViewPos - tFragPos);
        fragmentUV = doParallaxMapping(fragmentUV, tViewDir);
        if (fragmentUV.x < 0.0f || fragmentUV.y < 0.0f ||
            fragmentUV.x > 1.0f || fragmentUV.y > 1.0f)
        {
            discard;
        }
    }

    vec4 diffuse = vec4(MaterialParameters.diffuse, 1.0f);
    if (hasAlbedoMap)
    {
        vec4 diffuse_sample = texture(sampler2D(AlbedoMap, LinearRepeatSampler), fragmentUV);
        if (any(notEqual(diffuse.rgb, zero_vec)))
        {
            diffuse *= diffuse_sample;
        }
        else
        {
            diffuse = diffuse_sample;
        }
    }

    if (hasAlphaMap)
    {
        diffuse.a = texture(sampler2D(AlphaMap, LinearRepeatSampler), fragmentUV).r;
    }

    vec3 specular = MaterialParameters.specular;
    const bool noUboSpec = any(notEqual(specular.rgb, zero_vec));
    if (hasSpecularMap)
    {
        vec3 specular_sample = texture(sampler2D(SpecularMap, LinearRepeatSampler), fragmentUV).rgb;
        if (noUboSpec)
        {
            specular *= specular_sample;
        }
        else
        {
            specular = specular_sample;
        }
    }

    float roughness = MaterialParameters.roughness;
    if (hasRoughnessMap)
    {
        float roughness_sample = texture(sampler2D(RoughnessMap, LinearRepeatSampler), fragmentUV).r;
        if (roughness != 0.0f)
        {
            roughness *= roughness_sample;
        }
        else
        {
            roughness = roughness_sample;
        }
    }


    vec4 ambient = vec4(MaterialParameters.ambient, 1.0f);
    if (hasAmbientOcclusionMap)
    {
        vec4 ambient_sample = texture(sampler2D(AmbientOcclusionMap, LinearRepeatSampler), fragmentUV);
        if (any(notEqual(ambient.rgb, zero_vec)))
        {
            ambient *= ambient_sample;
        }
        else
        {
            ambient = ambient_sample;
        }
    }

    vec3 emissive = MaterialParameters.emissive;
    if (hasEmissiveMap)
    {
        float emissive_sample = texture(sampler2D(EmissiveMap, LinearRepeatSampler), fragmentUV).r;
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
        float metallic_sample = texture(sampler2D(MetallicMap, LinearRepeatSampler), fragmentUV).r;
        if (metallic != 0.0f)
        {
            metallic *= metallic_sample;
        }
        else
        {
            metallic = metallic_sample;
        }
    }

    lighting_input.uv = fragmentUV;
    lighting_input.diffuseColor = diffuse.rgb;
    lighting_input.specularColor = specular;
    lighting_input.roughness = roughness;
    lighting_input.metallic = metallic;

    LightingResult lit = Lighting(index_1d, lighting_input);

    vec3 out_color = (diffuse.rgb * ambient.rgb * baseDiffuse) + lit.Diffuse.rgb + lit.Specular.rgb + emissive.rgb;
    out_color = out_color / (out_color + vec3(1.0f));
    out_color = pow(out_color, vec3(1.0f / 2.20f));

    backbuffer = vec4(out_color, diffuse.a);
}