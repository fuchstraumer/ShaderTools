#pragma INTERFACE_OVERRIDE
layout (location = 0) in vec4 gColor;
layout (location = 0) out vec4 backbuffer;
#pragma END_INTERFACE_OVERRIDE

void main() {
    backbuffer = vec4(gColor.rgb, 0.2f);
}
