
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

float GetSpecular(Material material, vec4 V, vec4 L, vec4 N) {
    vec4 r = normalize(reflect(-L,N));
    float RdotV = max(dot(r,V),0.0f);
    return pow(RdotV, material.shininess);
}

float Attenuation(float range, float d) {
    return 1.0f - smoothstep(range * 0.75f, range, d);
}

float GetSpotLightCone(SpotLight light, vec4 L) {
    float minCos = cos(radians(light.SpotLightAngle));
    float maxCos = mix(minCos, 1.0f, 0.0f);
    float cosAngle = dot(light.Direction, -L);
    return smoothstep(minCos, maxCos, cosAngle);
}

LightingResult CalculatePointLight(PointLight light, Material material, vec4 V, vec4 P, vec4 N) {
    LightingResult result;
    vec4 L = light.PositionViewSpace - P;
    float dist = length(L);
    L = L / dist;

    float atten = Attenuation(light.Range, dist);

    result.Diffuse = light.Color * GetDiffuse(N,L) * atten * light.Intensity;
    result.Specular = light.Color * GetSpecular(material, V, L, N) * atten * light.Intensity;

    return result;
}

LightingResult CalculateDirectionalLight(DirectionalLight light, Material material, vec4 V, vec4 P, vec4 N) {
    LightingResult result;
    vec4 L = normalize(-light.DirectionViewSpace);
    result.Diffuse = light.Color * GetDiffuse(N, L) * light.Intensity;
    result.Specular = light.Color * GetSpecular(material, V, L, N) * light.Intensity;
    return result;
}

LightingResult CalculateSpotLight(SpotLight light, Material material, vec4 V, vec4 P, vec4 N) {
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

vec4 ClipToView(vec4 clip_pos, mat4 projection_matrix) {
    vec4 view = inverse(projection_matrix) * clip_pos;
    view /= view.w;
    return view;
}

Plane ComputePlane(vec3 p0, vec3 p1, vec3 p2) {
    Plane result;
    vec3 v0 = p1 - p0;
    vec3 v1 = p2 - p0;
    result.N = normalize(cross(v0,v1));
    result.d = dot(result.N, p0);
    return result;
}

bool SphereInsidePlane(Sphere sph, Plane p) {
    return dot(p.N, sph.c) - p.d < -sph.r;
}

bool PointInsidePlane(vec3 pt, Plane pl) {
    return dot(pl.N, pt) - pl.d < 0.0f;
}

bool ConeInsidePlane(Cone cone, Plane plane) {
    vec3 m = cross(cross(plane.N, cone.d), cone.d);
    vec3 Q = cone.T + cone.d * cone.h - m * cone.r;
    return PointInsidePlane(cone.T, plane) && PointInsidePlane(Q, plane);
}

bool SphereInsideFrustum(Sphere sphere, Frustum frustum, vec2 depth_range) {
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

float SqDistanceFromPtToAABB(vec3 p, AABB b) {
    float result = 0.0f;
    for (int i = 0; i < 3; ++i) {
        if (p[i] < b.Min[i]) {
            result += pow(b.Min[i] - p[i], 2.0f);
        }
        if (p[i] > b.Max[i]) {
            result += pow(p[i] - b.Max[i], 2.0f);
        }
    }
    return result;
}

bool SphereInsideAABB(Sphere sphere, AABB b) {
    float sq_distance = SqDistanceFromPtToAABB(sphere.c, b);
    return sq_distance <= sphere.r * sphere.r;
}

bool AABBIntersectAABB(AABB a, AABB b) {
    bvec4 result0 = greaterThan(a.Max,b.Min);
    bvec4 result1 = lessThan(a.Min,b.Max);
    return all(result0) && all(result1);
}

bool ConeInsideFrustum(Cone cone, Frustum frustum, vec2 depth_range) {
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

bool IntersectLinePlane(vec3 a, vec3 b, Plane plane, out vec3 q) {
    vec3 ab = b - a;
    float t  = (plane.d - dot(plane.N, a)) / dot(plane.N, ab);
    bool intersect = ( t >= 0.0f && t <= 1.0f);
    q = vec3(0.0f);
    if (intersect) {
        q = a + t * ab;
    }
    return intersect;
}
