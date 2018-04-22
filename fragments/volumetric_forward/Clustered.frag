
layout(early_fragment_tests) in;
#include "Structures.glsl"
#include "Functions.glsl"

#pragma USE_RESOURCES GlobalResources
#pragma USE_RESOURCES VolumetricForward
#pragma USE_RESOURCES SortResources
#pragma USE_RESOURCES VolumetricForwardLights

void main() {
    backbuffer = vec4(1.0f, 1.0f, 1.0f, 1.0f);
}
