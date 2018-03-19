// from https://github.com/SaschaWillems/Vulkan/blob/master/data/shaders/pbrtexture/pbrtexture.frag#L49

vec4 getAlbedo() {
    const vec2 powers(2.20f,1.0f);
    return pow(sgGetAlbedo(),powers.xxxy);
}

float D_GGX(float dotnh, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha*alpha;
    float denom = dotnh * dotnh * (alpha2 - 1.0f) + 1.0f;
    return alpha2 / (PI * denom * denom);
}

float G_SchlickSmithGGX(float dotnl, float dotnv, float roughness) {
    float r = (roughness + 1.0f);
    float k = (r * r) * 0.125f; // replaced divide with add
    float GL = dotnl / (dotnl * (1.0f - k) + k);
    float GV = dotnv / (dotnv * (1.0f - k) + k);
    return GL * GV;
}

vec3 F_Schlick(float costheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - costheta, 5.0f);
}

vec3 F_SchlickR(float costheta, vec3 f0, float roughness) {
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - costheta, 5.0f);
}

vec3 prefilteredReflection(vec3 R, float roughness) {
    const float MAX_REFLECTION_LOD = 9.0f;
    float lod = roughness * MAX_REFLECTION_LOD;
    float lodf = floor(lod);
    float lodc = ceil(lod);
    vec3 a = textureLod(prefilteredMap, R, lodf).rgb;
    vec3 b = textureLod(prefilteredMap, R, lodc).rgb;
    return mix(a,b,lod-lodf);
}

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness) {
    const float pi = 3.1415926535897932384626433832795f;
    vec3 H = normalize(V+L);
    float dotNH = clamp(dot(N,H),0.0f,1.0f);
    float dotNV = clamp(dot(N,V),0.0f,1.0f);
    float dotNL = clamp(dot(N,L),0.0f,1.0f);

    if (dotNL < 0.0) {
        return vec3(0.0);
    }

    float D = D_GGX(dotNH,roughness);
    float G = G_SchlickSmithGGX(dotNL,dotNV,roughness);
    float F = F_Schlick(dotNV,F0);
    vec3 specular = D * F * G / (4.0 * dotNL * dotNV + 0.001f);
    vec3 kd = (vec3(1.0 - F) * (1.0f - metallic));
    return (kd * getAlbedo() / pi + specular) * dotNL;
}

vec3 tangentFromNormalMap(vec2 uv) {
    vec3 norm = texture($NORMAL_TEXTURE, uv).xyz * 2.0f - 1.0f;
    vec3 q1 = dFdx(vPosition);
    vec3 q2 = dFdy(vPosition);
    vec2 st1 = dFdx(vUV);
    vec2 st2 = dFdy(vUV);
    return normalize(q1 * st2.t - q2 * st1.t);
}

vec3 perturbNormal() {
    vec3 n = normalize(vNormal);
    vec3 b = -normalize(cross(n,t));
    mat3 TBN = mat3(tangentFromNormalMap(vUV),b,n);
    return normalize(TBN * tangent_normal);
}