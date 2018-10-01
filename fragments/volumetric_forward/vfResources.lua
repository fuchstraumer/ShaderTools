
local dimensions = require "Dimensions";

ObjectSizes = {
    PointLight = 64,
    SpotLight = 96,
    DirectionalLight = 64,
    AABB = 32,
    Plane = 16,
    Frustum = 64,
    Sphere = 16,
    Cone = 32,
    LightingResult = 48,
    Material = 128
}

Resources = {
    GlobalResources = {
        matrices = {
            Type = "UniformBuffer",
            Members = {
                model = { "mat4", 0 },
                view = { "mat4", 1 },
                inverseView = { "mat4", 2 },
                projection = { "mat4", 3 },
                modelView = { "mat4", 4 },
                modelViewProjection = { "mat4", 5 },
                inverseTransposeModel = { "mat4", 6 },
                inverseTransposeModelView = { "mat4", 7 }
            }
        },
        globals = {
            Type = "UniformBuffer",
            Members = {
                viewPosition = { "vec4", 0 },
                mousePosition = { "vec2", 1 },
                windowSize = { "vec2", 2 },
                depthRange = { "vec2", 3 },
                frame = { "uint", 4 },
                exposure = { "float", 5 },
                gamma = { "float", 6 },
                brightness = { "float", 7 }
            }
        }
    },
    Material = {
        Tags = {
            "MaterialGroup"
        },
        MaterialParameters = {
            Type = "UniformBuffer",
            Members = {
                Data = { "Material", 0 }
            }
        },
        AlbedoMap = {
            Type = "SampledImage",
            FromFile = true
        },
        NormalMap = {
            Type = "SampledImage",
            FromFile = true
        },        
        AmbientOcclusionMap = {
            Type = "SampledImage",
            FromFile = true
        },
        MetallicRoughnessMap = {
            Type = "SampledImage",
            FromFile = true
        },
        EmissiveMap = {
            Type = "SampledImage",
            FromFile = true
        },
        LinearRepeatSampler = {
            Type = "Sampler",
            SamplerInfo = {
    
            }
        },
        LinearClampSampler = {
            Type = "Sampler",
            SamplerInfo = {
                AddressModeU = "ClampToEdge",
                AddressModeV = "ClampToEdge",
                AddressModeW = "ClampToEdge"
            }
        },
        AnisotropicSampler = {
            Type = "Sampler",
            AnisotropicSampler = {
                EnableAnisotropy = true,
                MaxAnisotropy = 8.0
            }
        }
    },
    VolumetricForward = {
        ClusterData = {
            Type = "UniformBuffer",
            Members = {
                GridDim = { "uvec3", 0 },
                ViewNear = { "float", 1 },
                ScreenSize = { "uvec2", 2 },
                NearK = { "float", 3 },
                LogGridDimY = { "float", 4 }
            },
            Qualifiers = "restrict readonly"
        },
        ClusterAABBs = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = dimensions.NumClusters()
                }, 0 }
            },
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                AssignLightsToClustersBVH = "readonly"
            }
        },
        ClusterFlags = {
            Type = "StorageTexelBuffer",
            Format = "r8ui",
            Size = dimensions.NumClusters(),
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                FindUniqueClusters = "readonly",
                ClusterSamples = "writeonly"
            }
        },
        PointLightIndexList = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.LightIndexListSize(),
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                AssignLightsToClustersBVH = "writeonly",
                Clustered = "readonly"
            }
        },
        SpotLightIndexList = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.LightIndexListSize(),
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                AssignLightsToClustersBVH = "writeonly",
                Clustered = "readonly"
            }
        },
        PointLightGrid = {
            Type = "StorageTexelBuffer",
            Format = "rg32ui",
            Size = dimensions.LightGridSize(),
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                AssignLightsToClustersBVH = "writeonly",
                Clustered = "readonly"
            }
        },
        SpotLightGrid = {
            Type = "StorageTexelBuffer",
            Format = "rg32ui",
            Size = dimensions.LightGridSize(),
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                AssignLightsToClustersBVH = "writeonly",
                Clustered = "readonly"
            }
        },
        PointLightIndexCounter = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 1,
            Qualifiers = "restrict"
        },
        SpotLightIndexCounter = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 1,
            Qualifiers = "restrict"
        },
        UniqueClustersCounter = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 1,
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                UpdateClusterIndirectArgs = "readonly"
            }
        },
        UniqueClusters = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.NumClusters(),
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                AssignLightsToClustersBVH = "readonly",
                FindUniqueClusters = "writeonly"
            }
        }
    },
    VolumetricForwardLights = {
        LightCounts = {
            Type = "UniformBuffer",
            Members = {
                NumPointLights = { "uint", 0 },
                NumSpotLights = { "uint", 1 },
                NumDirectionalLights = { "uint", 2 }
            }
        },
        PointLights = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "PointLight",
                    NumElements = dimensions.NumPointLights()
                }, 0 }
            },
            -- PerUsageQualifiers: qualifiers to apply only to single shaders in the pack
            -- Other qualifiers are applied pack-wide
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                Clustered = "readonly",
                ReduceLightsAABB = "readonly",
                ComputeMortonCodes = "readonly",
                AssignLightsToClustersBVH = "readonly"
            }
        },
        SpotLights = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "SpotLight",
                    NumElements = dimensions.NumSpotLights()
                }, 0 }
            },
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                Clustered = "readonly",
                ReduceLightsAABB = "readonly",
                ComputeMortonCodes = "readonly",
                AssignLightsToClustersBVH = "readonly"
            }
        },
        DirectionalLights = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "DirectionalLight",
                    NumElements = dimensions.NumDirectionalLights()
                }, 0 }
            },
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                Clustered = "readonly",
                ReduceLightsAABB = "readonly",
                ComputeMortonCodes = "readonly"
            }
        }
    },
    IndirectArgsSet = {
        IndirectArgs = {
            Type = "StorageBuffer",
            Members = {
                NumThreadGroupsX = { "uint", 0 },
                NumThreadGroupsY = { "uint", 1 },
                NumThreadGroupsZ = { "uint", 2 }
            },
            Qualifiers = "restrict"
        }       
    },
    SortResources = {
        DispatchParams = {
            Type = "UniformBuffer",
            Members = {
                NumThreadGroups = { "uvec3", 0 },
                Padding0 = { "uint", 1 },
                NumThreads = { "uvec3", 2 },
                Padding1 = { "uint", 3 }
            }
        },
        ReductionParams = {
            Type = "UniformBuffer",
            Members = {
                NumElements = { "uint", 0 }
            }
        },
        SortParams = {
            Type = "UniformBuffer",
            Members = {
                NumElements = { "uint", 0 },
                ChunkSize = { "uint", 1 }
            }
        },
        LightAABBs = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = dimensions.NumLightAABBs()
                }, 0 }
            },
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                ComputeMortonCodes = "readonly"
            }
        },
        PointLightMortonCodes = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.NumPointLights(),
            Qualifiers = "restrict"
        },
        SpotLightMortonCodes = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.NumSpotLights(),
            Qualifiers = "restrict"
        },
        PointLightIndices = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.NumPointLights(),
            Qualifiers = "restrict"
        },
        SpotLightIndices = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.NumSpotLights(),
            Qualifiers = "restrict"
        }
    },
    MergeSortResources = {
        InputKeys = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 8 * 256,
            Qualifiers = "restrict readonly",
            Tags = { "DoNotCreate" }
        },
        InputValues = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 8 * 256,
            Qualifiers = "restrict readonly",
            Tags = { "DoNotCreate" }
        },
        OutputKeys = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 8 * 256,
            Qualifiers = "restrict writeonly",
            Tags = { "DoNotCreate" }
        },
        OutputValues = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 8 * 256,
            Qualifiers = "restrict writeonly",
            Tags = { "DoNotCreate" }
        }, 
        MergePathPartitions = {
            Type = "StorageTexelBuffer",
            Format = "r32i",
            Size = dimensions.MaxMergePathPartitions(),
            Qualifiers = "restrict"
        }
    },
    BVHResources = {
        BVHParams = {
            Type = "UniformBuffer",
            Members = {
                PointLightLevels = { "uint", 0 },
                SpotLightLevels = { "uint", 1 },
                ChildLevel = { "uint", 2 }
            }
        },
        PointLightBVH = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = dimensions.NumPointLights()
                }, 0 }
            },
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                AssignLightsToClustersBVH = "readonly"
            }
        },
        SpotLightBVH = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = dimensions.NumSpotLights()
                }, 0 }
            },
            Qualifiers = "restrict",
            PerUsageQualifiers = {
                AssignLightsToClustersBVH = "readonly"
            }
        }
    }
}
