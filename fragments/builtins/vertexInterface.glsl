#pragma VERT_INTERFACE_BEGIN
in vec3 position;
in vec3 normal;
in vec3 tangent;
in vec2 uv;
out vec3 vPosition;
out vec3 vNormal;
out vec3 vTangent;
out vec2 vUV;
#pragma VERT_INTERFACE_END

#pragma VERT_INTERFACE_PACKED_BEGIN
in vec3 position;
in vec3 normal;
in vec3 tangent;
in vec2 uv;
out vec4 positionXYZ_normalX;
out vec4 normalYZ_tangentXY;
out vec4 tangentZ_uvXY_dummyW;