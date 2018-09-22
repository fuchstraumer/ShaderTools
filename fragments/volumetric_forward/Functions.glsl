
vec3 ExpandNormal(vec3 n) {
    return n * 2.0f - 1.0f;
}

vec4 NormalMapping(mat3 tbn, vec3 _sample) {
    vec3 normal = ExpandNormal(_sample);
    normal *= tbn;
    return normalize(vec4(normal, 0.0f));
}

float GetDiffuse(vec4 n, vec4 l) {
    float NdotL = max(dot(n,l), 0.0f);
    return NdotL;
}

float GetSpecular(in Material material, vec4 V, vec4 L, vec4 N) {
    vec4 r = normalize(reflect(-L,N));
    float RdotV = max(dot(r,V),0.0f);
    return pow(RdotV, material.reflectance);
}

float Attenuation(float range, float d) {
    return 1.0f - smoothstep(range * 0.75f, range, d);
}

float GetSpotLightCone(in SpotLight light, vec4 L) {
    float minCos = cos(radians(light.SpotLightAngle));
    float maxCos = mix(minCos, 1.0f, 0.0f);
    float cosAngle = dot(light.Direction, -L);
    return smoothstep(minCos, maxCos, cosAngle);
}

LightingResult CalculatePointLight(in const PointLight light, in Material material, vec4 V, vec4 P, vec4 N) {
    LightingResult result;
    vec4 L = light.PositionViewSpace - P;
    float dist = length(L);
    L = L / dist;

    float atten = Attenuation(light.Range, dist);

    result.Diffuse = light.Color * GetDiffuse(N,L) * atten * light.Intensity;
    result.Specular = light.Color * GetSpecular(material, V, L, N) * atten * light.Intensity;

    return result;
}

LightingResult CalculateDirectionalLight(in const DirectionalLight light, in Material material, vec4 V, vec4 P, vec4 N) {
    LightingResult result;
    vec4 L = normalize(-light.DirectionViewSpace);
    result.Diffuse = light.Color * GetDiffuse(N, L) * light.Intensity;
    result.Specular = light.Color * GetSpecular(material, V, L, N) * light.Intensity;
    return result;
}

LightingResult CalculateSpotLight(in const SpotLight light, in Material material, vec4 V, vec4 P, vec4 N) {
    LightingResult result;

    vec4 L = light.PositionViewSpace - P;
    float dist = length(L);
    L /= dist;

    float atten = Attenuation(light.Range, dist);
    float spot_intensity = GetSpotLightCone(light, L);

    result.Diffuse = light.Color * GetDiffuse(N, L) * atten * spot_intensity * light.Intensity;
    result.Specular = light.Color * GetSpecular(material, V, L, N) * atten * spot_intensity * light.Intensity;

    return result;
}

vec4 ClipToView(in vec4 clip_pos, in mat4 projection_matrix) {
    vec4 view = inverse(projection_matrix) * clip_pos;
    view /= view.w;
    return view;
}

Plane ComputePlane(in vec3 p0, in vec3 p1, in vec3 p2) {
    Plane result;
    result.N = normalize(cross(p1 - p0, p2 - p0));
    result.d = dot(result.N, p0);
    return result;
}

bool SphereInsidePlane(in Sphere sph, in Plane p) {
    return dot(p.N, sph.c) - p.d < -sph.r;
}

bool PointInsidePlane(in vec3 pt, in Plane pl) {
    return dot(pl.N, pt) - pl.d < 0.0f;
}

bool ConeInsidePlane(in Cone cone, in Plane plane) {
    vec3 m = cross(cross(plane.N, cone.d), cone.d);
    vec3 Q = cone.T + cone.d * cone.h - m * cone.r;
    return PointInsidePlane(cone.T, plane) && PointInsidePlane(Q, plane);
}

bool SphereInsideFrustum(in Sphere sphere, in Frustum frustum, in vec2 depth_range) {
    if (sphere.c.z - sphere.r > depth_range.x || sphere.c.z + sphere.r < depth_range.y) {
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        if (SphereInsidePlane(sphere, frustum.planes[i])) {
            return false;
        }
    }

    return true;
}

bool SphereInsideAABB(in Sphere sphere, in AABB b) {
    float result = 0.0f;
    for (uint i = 0; i < 3; ++i) {
        if (sphere.c[i] < b.Min[i]) {
            result += (sphere.c[i] - b.Min[i]) * (sphere.c[i] - b.Min[i]);
        }
        else if (sphere.c[i] > b.Max[i]) {
            result += (sphere.c[i] - b.Max[i]) * (sphere.c[i] - b.Max[i]);
        }
    }
    return (result <= sphere.r * sphere.r);
}

bool AABBIntersectAABB(AABB a, AABB b) {
    bvec4 result0 = greaterThan(a.Max,b.Min);
    bvec4 result1 = lessThan(a.Min,b.Max);
    return all(result0) && all(result1);
}

bool ConeInsideFrustum(in Cone cone, in Frustum frustum, in vec2 depth_range) {
    Plane near_plane;
    near_plane.N = vec3(0.0f, 0.0f,-1.0f);
    near_plane.d = -depth_range.x;
    Plane far_plane;
    far_plane.N = vec3(0.0f, 0.0f, 1.0f);
    far_plane.d = depth_range.y;

    if (ConeInsidePlane(cone, near_plane) || ConeInsidePlane(cone, far_plane)) {
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        if (ConeInsidePlane(cone, frustum.planes[i])) {
            return false;
        }
    }

    return true;
}

bool IntersectLinePlane(in vec3 a, in vec3 b, in Plane plane, out vec3 q) {
    vec3 ab = b - a;
    float t  = (plane.d - dot(plane.N, a)) / dot(plane.N, ab);
    bool intersect = ( t >= 0.0f && t <= 1.0f);
    q = vec3(0.0f);
    if (intersect) {
        q = a + t * ab;
    }
    return intersect;
}
