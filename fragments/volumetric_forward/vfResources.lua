local dimensions = require("Dimensions");

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
                projection = { "mat4", 2 },
                normal = { "mat4", 3 }
            }
        },
        globals = {
            Type = "UniformBuffer",
            Members = {
                viewPosition = { "vec4", 0 },
                mousePosition = { "vec2", 1 },
                windowSize = { "vec2", 2 },
                depthRange = { "vec2", 3 },
                frame = { "uint", 4 }
            }
        },
        lightingData = {
            Type = "UniformBuffer",
            Members = {
                Exposure = { "float", 0 },
                Gamma = { "float", 1 }
            }
        }
    },
    MaterialResources = {
        MaterialParameters = {
            Type = "UniformBuffer",
            Members = {
                Data = { "Material", 0 }
            }
        },
        DiffuseMap = {
            Type = "SampledImage",
            -- Images from file don't have any info set here:
            -- all we do is generate a suitable descriptor binding
            -- for what will eventually be used
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
        RoughnessMap = {
            Type = "SampledImage",
            FromFile = true
        },
        MetallicMap = {
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
                Near = { "float", 3 },
                LogGridDimY = { "float", 4 }
            }
        },
        ClusterAABBs = {
            Type = "StorageBuffer",
            Members = {
                Data = { {
                    Type = "Array",
                    ElementType = "AABB",
                    NumElements = dimensions.NumClusters()
                }, 0 }
            }
        },
        ClusterFlags = {
            Type = "StorageTexelBuffer",
            Format = "r8ui",
            Size = dimensions.NumClusters()
        },
        PointLightIndexList = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.LightIndexListSize()
        },
        SpotLightIndexList = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.LightIndexListSize()
        },
        PointLightGrid = {
            Type = "StorageTexelBuffer",
            Format = "rg32ui",
            Size = dimensions.LightGridSize()
        },
        SpotLightGrid = {
            Type = "StorageTexelBuffer",
            Format = "rg32ui",
            Size = dimensions.LightGridSize()
        },
        PointLightIndexCounter = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 1
        },
        SpotLightIndexCounter = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 1
        },
        UniqueClustersCounter = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 1
        },
        UniqueClusters = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.NumClusters()
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
            }
        }       
    },
    SortResources = {
        DispatchParams = {
            Type = "UniformBuffer",
            Members = {
                NumThreadGroups = { "uvec3", 0 },
                NumThreads = { "uvec3", 1 }
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
            }
        },
        PointLightMortonCodes = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.NumPointLights()
        },
        SpotLightMortonCodes = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.NumSpotLights()
        },
        PointLightIndices = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.NumPointLights()
        },
        SpotLightIndices = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.NumSpotLights()
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
            }
        }
    }
}
