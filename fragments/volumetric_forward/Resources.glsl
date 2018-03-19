#pragma BEGIN_RESOURCES VOLUMETRIC_FORWARD
UNIFORM_BUFFER cluster_data {
    uvec3 GridDim;
    float ViewNear;
    uvec2 Size;
    float Near;
    float LogGridDimY;
} ClusterData;

U_IMAGE_BUFFER r8ui ClusterFlags;
U_IMAGE_BUFFER r32ui PointLightIndexList;
U_IMAGE_BUFFER r32ui SpotLightIndexList;
U_IMAGE_BUFFER rg32ui PointLightGrid;
U_IMAGE_BUFFER rg32ui SpotLightGrid;
U_IMAGE_BUFFER r32ui UniqueClustersCounter;
U_IMAGE_BUFFER r32ui UniqueClusters;

#pragma END_RESOURCES VOLUMETRIC_FORWARD

#pragma BEGIN_RESOURCES VOLUMETRIC_FORWARD_LIGHTS

UNIFORM_BUFFER light_counts {
    uint NumPointLights;
    uint NumSpotLights;
    uint NumDirectionalLights;
} LightCounts;

STORAGE_BUFFER std430 point_lights {
    PointLight Data[];
} PointLights;

STORAGE_BUFFER std430 spot_lights {
    SpotLight Data[];
} SpotLights;

STORAGE_BUFFER std430 directional_lights {
    DirectionalLight Data[];
} DirectionalLights;

#pragma END_RESOURCES VOLUMETRIC_FORWARD_LIGHTS

#pragma BEGIN_RESOURCES INDIRECT_ARGS
UNIFORM_BUFFER indirect_arguments {
    uint NumThreadGroupsX;
    uint NumThreadGroupsY;
    uint NumThreadGroupsZ;
} IndirectArgs;

#pragma END_RESOURCES INDIRECT_ARGS

#pragma BEGIN_RESOURCES SORT_RESOURCES
UNIFORM_BUFFER dispatch_params {
    uvec3 NumThreadGroups;
    uvec3 NumThreads;
} DispatchParams;

UNIFORM_BUFFER reduction_params {
    uint NumElements;
} ReductionParams;

UNIFORM_BUFFER sort_params {
    uint NumElements;
    uint ChunkSize;
} SortParams;

STORAGE_BUFFER std430 cluster_aabbs {
    AABB Data[];
} ClusterAABBs;

STORAGE_BUFFER std430 global_aabb {
    AABB Data[];
} LightAABBs;

U_IMAGE_BUFFER r32ui PointLightIndexCounter;
U_IMAGE_BUFFER r32ui SpotLightIndexCounter;
U_IMAGE_BUFFER r32ui PointLightIndexList;
U_IMAGE_BUFFER r32ui SpotLightIndexList;
U_IMAGE_BUFFER rg32ui PointLightGrid;
U_IMAGE_BUFFER rg32ui SpotLightGrid;
U_IMAGE_BUFFER r32ui InputKeys;
U_IMAGE_BUFFER r32ui InputValues;
U_IMAGE_BUFFER r32ui OutputKeys;
U_IMAGE_BUFFER r32ui OutputValues;
I_IMAGE_BUFFER r32i MergePathPartitions;

#pragma END_RESOURCES SORT_RESOURCES

#pragma BEGIN_RESOURCES BVH_RESOURCES
UNIFORM_BUFFER bvh_params {
    uint PointLightLevels;
    uint SpotLightLevels;
    uint ChildLevel;
} BVHParams;

STORAGE_BUFFER std430 point_light_bvh {
    AABB Data[];
} PointLightBVH;

STORAGE_BUFFER std430 spot_light_bvh {
    AABB Data[];
} SpotLightBVH;

#pragma END_RESOURCES BVH_RESOURCES

#pragma BEGIN_RESOURCES MORTON_RESOURCES

U_IMAGE_BUFFER r32ui PointLightMortonCodes;
U_IMAGE_BUFFER r32ui PointLightIndices;
U_IMAGE_BUFFER r32ui SpotLightMortonCodes;
U_IMAGE_BUFFER r32ui SpotLightIndices;

#pragma END_RESOURCES MORTON_RESOURCES

#pragma BEGIN_RESOURCES MATERIAL_RESOURCES

TEXTURE_2D ambientMap;
TEXTURE_2D diffuseMap;
TEXTURE_2D specularMap;
TEXTURE_2D opacityMap;
TEXTURE_2D bumpMap;
TEXTURE_2D displacementMap;
TEXTURE_2D roughnessMap;
TEXTURE_2D metallicMap;
TEXTURE_2D sheenMap;
TEXTURE_2D emissiveMap;
TEXTURE_2D normalMap;

UNIFORM_BUFFER material_texture_flags {
    bool Ambient;
    bool Diffuse;
    bool Specular;
    bool Opacity;
    bool BumpMap;
    bool Displacement;
    bool Roughness;
    bool Metallic;
    bool Sheen;
    bool Emissive;
    bool NormalMap;
} TextureFlags;

UNIFORM_BUFFER material_data {
    vec4 ambient;
    vec4 diffuse; 
    vec4 specular;
    vec4 transmittance;
    vec4 emission;
    float shininess;
    float ior;
    float alpha;
    int illuminationModel;
    float roughness;
    float metallic;
    float sheen;
    float clearcoatThickness;
    float clearcoatRoughness;
    float anisotropy;
    float anisotropyRotation;
    float globalAmbient;
} Material;

#pragma END_RESOURCES MATERIAL_RESOURCES

