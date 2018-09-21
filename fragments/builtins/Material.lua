local Material = {
    function()
        MaterialResources = {
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
        return MaterialResources
    end
}

return Material
