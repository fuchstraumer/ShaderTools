
SPC const bool HasDiffuse = true;
#include "Structures.glsl"
#pragma USE_RESOURCES GlobalResources
#pragma USE_RESOURCES VolumetricForward
#pragma USE_RESOURCES Material

void main() {
    float alpha = 1.0f;
    if (HasDiffuse) {
        alpha = texture(sampler2D(AlbedoMap, LinearRepeatSampler), vUV).a;
    }
    else {
        alpha = MaterialParameters.Data.baseColor.a;
    }

    if (alpha < 0.95f) {
        discard;
    }

}
