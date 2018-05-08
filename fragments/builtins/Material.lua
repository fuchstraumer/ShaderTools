Resources = {
    materialResources = {
        MaterialParameters = {
            Type = "UniformBuffer",
            Members = {
                Material = { "Material", 0 }
            }
        },
        DiffuseMap = {
            Type = "SampledImage",
            ImageInfo = {
                -- Images from file don't have any info set here:
                -- all we do is generate a suitable descriptor binding
                -- for what will eventually be used
                FromFile = true
            }
        },
        NormalMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
        },
        RoughnessMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
        },
        MetallicMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
        },
        BumpMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
        },
        SpecularMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
        },
        SpecularHighlightMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
        },
        DisplacementMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
        },
        AlphaMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
        },
        ReflectionMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
        },
        SheenMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
        },
        EmissiveMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
        },
        AmbientMap = {
            Type = "SampledImage",
            ImageInfo = {
                FromFile = true
            }
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