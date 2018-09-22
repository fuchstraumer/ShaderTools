
local dimensions = require("Functions")
local mtl = require("Material")

Resources = {
    GlobalResources = {
        UBO = {
            Type = "UniformBuffer",
            Members = {
                model = { "mat4", 0 },
                view = { "mat4", 1 },
                projectionClip = { "mat4", 2 },
                normal = { "mat4", 3 },
                viewPosition = { "vec4", 4 },
                depth = { "vec2", 5 },
                numLights = { "uint", 6 }
            }
        }
    },
    ClusteredForward = {
        Flags = {
            Type = "StorageTexelBuffer",
            Format = "r8ui",
            Size = dimensions.TotalTileCount(),
            Qualifiers = "restrict"
        },
        lightBounds = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.NumLights * 6,
            Qualifiers = "restrict"
        },
        lightCounts = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.TotalTileCount(),
            Qualifiers = "restrict"
        },
        lightCountTotal = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 1,
            Qualifiers = "restrict"
        },
        lightCountOffsets = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.TotalTileCount(),
            Qualifiers = "restrict"
        },
        lightList = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 1024 * 1024,
            Qualifiers = "restrict"
        },
        lightCountCompare = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = dimensions.TotalTileCount(),
            Qualifiers = "restrict"
        },
        -- Tags can be used by frontends to specify certain behavior.
        -- Here, I'll use these tags to know that this group has resources that 
        -- should be zero-initialized and cleared per pass/execution run
        Tags = { "InitializeToZero", "ClearAtEndOfPass" }
    },
    Lights = {
        positionRanges = {
            Type = "StorageTexelBuffer",
            Format = "rgba32f",
            Size = dimensions.NumLights,
            Qualifiers = "restrict",
            Tags = { "HostGeneratedData" }
        },
        lightColors = {
            Type = "StorageTexelBuffer",
            Format = "rgba8",
            Size = dimensions.NumLights,
            Qualifiers = "restrict readonly",
            Tags = { "HostGeneratedData" }
        }
    },
    Material = {
        MaterialParameters = {
            Type = "UniformBuffer",
            Members = {
                baseColor = { "vec4",  0 },
                emissive = { "vec4", 1 },
                roughness = { "float", 2 },
                metallic = { "float", 3 },
                reflectance = { "float", 4 },
                ambientOcclusion = { "float", 5 },
                clearCoat = { "float", 6 },
                clearCoatRoughness = { "float", 7 },
                anisotropy = { "float", 8 },
                anisotropyDirection = { "vec3", 9 },
                thickness = { "float", 10 },
                subsurfacePower = { "float", 11 },
                subsurfaceColor = { "vec4", 12 },
                sheenColor = { "vec4", 13 },
                normal = { "vec4", 14 }
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
    }
}
