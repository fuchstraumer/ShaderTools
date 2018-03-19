#pragma RESOURCES_BEGIN

$TEXTURE_CUBE irradianceMap;
$TEXTURE_CUBE prefilteredMap;
$TEXTURE_CUBE brdfLut;

struct Light {
    vec4 position;
    vec4 color; // alpha component stores radius
};

$UNIFORM_BUFFER lights_buffer_object {
    Light lights[sg_MaxLights]
} lbo;

$SPC const float A = 0.15f;
$SPC const float B = 0.50f;
$SPC const float C = 0.10f;
$SPC const float D = 0.20f;
$SPC const float E = 0.02f;
$SPC const float F = 0.30f;

$PUSH_CONSTANT_ITEM vec4 curve;

#pragma RESOURCES_END

#pragma FEATURE_SUPPLIED_RESOURCES
$TEXTURE_2D normals;
$TEXTURE_2D metallic;
$TEXTURE_2D roughness;
#pragma FEATURE_SUPPLIED_RESOURCES_END

#pragma MODEL_BEGIN
#include <lighting_functions_pbr.glsl>


vec3 tonemap(vec3 x) {
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec4 lightingModel() {

    vec3 N = perturbNormal();
    vec3 V = normalize(globals.viewPosition - vPosition);
    vec3 R = reflect(-V, N);

    float metallic = texture(metallic,vUV).r;
    float roughness = texture(roughness,vUV).r;

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, getAlbedo(), metallic);

    vec3 lo = vec3(0.0);
    for (int i = 0; i < sg_MaxLights; ++i) {
        vec3 l = normalize(lbo.lights[i].position.xyz - vPosition);
        lo += specularContribution(l,V,N,F0,metallic,roughness);
    }

    vec2 brdf = texture(brdfLUT, vec2(max(dot(N,V),0.0f),roughness)).rg;
    vec3 reflection  = prefilteredReflection(R,roughness).rgb;
    vec3 irradiance = texture(irradiance,N).rgb;

    vec3 diffuse = irradiance * getAlbedo();

    vec3 F = F_SchlickR(max(dot(N,V),0.0f),F0,roughness);
    vec3 specular = reflection * (F * brdf.x + brdf.y);
    vec3 kD = 1.0f - F;
    kD *= 1.0f - metallic;
    vec3 ambient = (kD * diffuse + specular) * texture(ao, vUV).rrr;
    vec3 color = ambient + lo;

    color = tonemap(color * Exposure());
    color = color * (1.0f / tonemap(vec3(11.2f)));
    color = pow(color, vec3(1.0f / Gamma()))
    
    return vec4(color, 1.0f);

}

#pragma MODEL_END