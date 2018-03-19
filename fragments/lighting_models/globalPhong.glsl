#pragma RESOURCES_BEGIN
$UNIFORM_BUFFER lights_data {
    vec4 lightPosition;
    vec4 lightColor;
} lightingData;
#pragma RESOURCES_END

#pragma MODEL_BEGIN
vec4 lightingModel() {
    const float ambient_strength = 0.3f;
    vec3 ambient = ambient_strength * lightingData.lightColor.xyz;

    const float diffuse_strength = 0.6f;
    vec3 light_direction = normalize(lightingData.lightPosition.xyz - vPosition);
    float diff = max(dot(vNormal, light_direction), 0.0f);
    vec3 diffuse = diffuse_strength * diff * lightingData.lightPosition.xyz;

    float specular_strength = 0.3f;
    vec3 view_direction = normalize(globals.viewPositions.xyz - vPosition);
    vec3 reflect_direction = reflect(-light_direction, vNormal);
    float spec = pow(max(dot(view_direction, reflect_direction), 0.0f), 32.0f);
    vec3 specular = specular_strength * spec * lightingData.lightPosition.xyz;

    return sgGetAlbedo() * vec4(ambient + diffuse + specular, 1.0f); 
}
#pragma MODEL_END

#pragma DEFERRED_MODEL_BEGIN
vec4 lightingModel(vec4 pos, vec4 normal) {
    const float ambient_strength = 0.3f;
    vec3 ambient = ambient_strength * lightingData.lightColor.xyz;

    const float diffuse_strength = 0.6f;
    vec3 light_direction = normalize(lightingData.lightPosition.xyz - pos);
    float diff = max(dot(normal, light_direction), 0.0f);
    vec3 diffuse = diffuse_strength * diff * lightingData.lightPosition.xyz;

    float specular_strength = 0.3f;
    vec3 view_direction = normalize(globals.viewPositions.xyz - pos);
    vec3 reflect_direction = reflect(-light_direction, normal);
    float spec = pow(max(dot(view_direction, reflect_direction), 0.0f), 32.0f);
    vec3 specular = specular_strength * spec * lightingData.lightPosition.xyz;

    return vec4(ambient + diffuse + specular, 1.0f); 
}
#pragma DEFERRED_MODEL_END