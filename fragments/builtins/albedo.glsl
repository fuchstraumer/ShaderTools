#pragma GET_ALBEDO_SPEC_CONSTANT
$SPC const float sg_ConstantAlbedo = vec4(0.0f, 1.0f, 0.1f, 1.0f);
vec4 sgGetAlbedo() {
    return sg_ConstantAlbedo;
}
#pragma END_GET_ALBEDO_SPEC_CONSTANT

#pragma GET_ALBEDO_PUSH_CONSTANT
$PUSH_CONSTANT_ITEM vec4 sg_PushAlbedo
vec4 sgGetAlbedo() {
    return $PUSH_CONSTANT.sg_PushAlbedo;
}
#pragma END_GET_ALBEDO_PUSH_CONSTANT

#pragma GET_ALBEDO_TEXTURE
$TEXTURE_2D albedo;
vec4 sgGetAlbedo() {
    return texture(albedo,vUV);
}
#pragma END_ALBEDO_TEXTURE
