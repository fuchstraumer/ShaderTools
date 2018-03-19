#pragma RESOURCES_BEGIN

struct Light {
    vec4 position;
    vec4 color; // alpha component stores radius
};

uniform lights_buffer_object {
    Light lights[sg_MaxLights]
} lbo;

#pragma RESOURCES_END

#pragma MODEL_BEGIN
vec4 lightingModel() { 
    vec4 result;
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        vec3 light_dir = lbo.lights[i].position.xyz - vPosition;
        float light_dist = length(light_dir);
        light_dir = normalize(light_dir);

        vec3 view_dir = globals.viewPosition.xyz - vPosition;
        view_dir = normalize(view_dir);

        float attenuation = lbo.lights[i].radius / (pow(light_dist, 2.0f) + 1.0f);

        float ndotl = max(0.0f, dot(vNormal, light_dir));
        vec3 diff = lbo.lights[i].color.rgb * color * ndotl * attenuation;

        vec3 reflection = reflect(-light_dir, vNormal);
        float ndotr = max(0.0, dot(reflection, view_dir));
        vec3 specular = lbo.lights[i].color.rgb * 0.10f * pow(ndotr, 32.0f) * attenuation;
        result += vec4(specular + diff);
    }
    return result;
}
#pragma MODEL_END