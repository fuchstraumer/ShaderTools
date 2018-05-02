--[[
    This test file is run as part of the test pack, to test the validity
    of all resource types. It's a unit test covering all descriptor types
    and several variations of the same descriptor type
]]
ObjectSizes = {
    PointLight = 64,
    AABB = 32
}

Resources = {
    MaterialResources = {
        MaterialData = {
            -- If using custom types in a UniformBuffer, add their size in bytes
            -- to "ObjectSizes" for best results. The size can potentially be calculated
            -- during binding generation, but this is not a guarantee.
            Type = "UniformBuffer",
            Members = {
                -- the pair of "type", "number" is required: "number" represents
                -- the members index (as the offset of it's field relative to the start)
                -- Without these, ordering would not be preserved between this script
                -- and the resulting GLSL
                ambient = { "vec4", 0 },
                diffuse = { "vec4", 1 },
                specular = { "vec4", 2 },
                transmittance = { "vec4", 3 },
                emission = { "vec4", 4 },
                shininess = { "float", 5 },
                ior = { "float", 6 },
                alpha = { "float", 7 },
                illuminationModel = { "int", 8 },
                roughness = { "float", 9 },
                metallic = { "float", 10 },
                sheen = { "float", 11 },
                clearcoatThickness = { "float", 12 },
                clearcoatRoughness = { "float", 13 },
                anisotropy = { "float", 14 },
                anisotropyRotation = { "float", 15 },
                padding = { "float", 16 }
            }
        },
        diffuseMap = {
            -- CombinedImageSamplers require both an Image and Sampler info
            -- member be specified in the resource script
            Type = "CombinedImageSampler",
            ImageInfo = {
                --[[
                    Potential image types are:
                    1D, 2D, 3D, 2D_Array, Cubemap
                ]]
                ImageType = "2D",
                -- Specifying this as "FromFile" tells the parser
                -- that we only know the ImageType right now: the rest
                -- will depend upon the loaded image.
                FromFile = true,
            },
            SamplerInfo = {
                -- Disabled by default. Max is usually 16.0, some 
                -- hardware supports up to 64.0
                EnableAnisotropy = true,
                MaxAnisotropy = 4.0
            }
        }
        ClampedSampler = {
            -- Sampler objects only require the sampler info
            -- struct. There are no required members, however.
            Type = "Sampler",
            SamplerInfo = {
                --[[
                    Potential options for Mag/Min filter are:
                    "Linear", "Nearest", "CubicImg"
                    Default is linear.
                ]]
                MagFilter = "CubicImg",
                MinFilter = "Nearest",
                --[[
                    Potential MipMapMode values are:
                    "Nearest", "Linear"
                    Default is linear
                ]]
                MipMapMode = "Nearest",
                --[[
                    Potential options for address modes are:
                    "ClampToEdge" "ClampToBorder" "Repeat"
                    "MirroredRepeat" "MirroredClampToEdge"
                    Default is "Repeat"
                ]]
                AddressModeU = "ClampToBorder",
                AddressModeV = "ClampToBorder",
                AddressModeW = "ClampToEdge",
                EnableAnisotropy = true,
                MaxAnisotropy = 4.0,
                -- Enable/disable compare ops. if true, "CompareOp" field
                -- is parsed. Otherwise it is ignored.
                CompareEnable = true,
                --[[
                    Potential compare ops are:
                    "Never", "Less", "Equal", "LessOrEqual",
                    "Greater", "NotEqual", "GreaterOrEqual", "Always"
                ]]
                CompareOp = "LessOrEqual",
                -- MinLod and MaxLod specify the min and max mipmap levels
                -- to use. Default is 0.0, 0.0 (no mips)
                MinLod = 0.0,
                MaxLod = 1.0,
                --[[
                    Potential border colors are:
                    "FloatTransparentBlack", "IntTransparentBlack", 
                    "IntOpaqueBlack", "FloatOpaqueBlack",
                    "FloatOpaqueWhite", "IntOpaqueWhite"
                    Default is FloatOpaqueWhite
                ]]
                BorderColor = "FloatTransparentBlack",
                -- Unnormallized coordinates means allow addressing
                -- outside the 0.0f-1.0f conventional range. Disabled 
                -- by default.
                UnnormalizedCoordinates = true
            }
        },
        NormalMap = {
            Type = "SampledImage",
            ImageOptions = {
                --[[
                    Potential size class options are:
                    "Absolute" (loaded from a file, usually),
                    "SwapchainRelative", "ViewportRelative"
                    Viewport relative is for the active viewport, potentially
                    a subsection of the swapchain's size.
                    TODO is input relative sizing.
                ]]
                SizeClass = "SwapchainRelative",
                ImageType = "2D_Array",
                -- Having specified 2D_Array type, and SwapchainRelative sizing,
                -- we need to specify the mip levels and array layers as they won't
                -- be loaded from file.
                MipLevels = 3,
                ArrayLayers = 4,
                -- We also need to specify the format when not loading from file. See
                -- util/ResourceFormats.cpp for the supported format strings: there
                -- are far too many to list here, unfortunately. Be aware that this list
                -- of formats doesn't account varying support across different hardware!
                Format = "rgba8",
                --[[
                    Sample count values are given as integers from 
                    1-64, as factors of 2
                    1, 2, 4, 8, 16, 32, 64
                    Most hardware caps at 16
                ]]
                SampleCount = 8
            }
        }
    },
    ComputeResources = {
        -- A storage image can be written to but cannot be read from
        -- or used with a sampler. As such, it requries an ImageInfo member
        DemoStorageImage = {
            Type = "StorageImage",
            ImageOptions = {
                -- This could be SwapchainRelative, or ViewportRelative: storage
                -- images are good for compute shader outputs, and we could be using
                -- this to output a "screenshot", for example!
                SizeClass = "Absolute",
                ImageType = "2D",
                -- 1 means no Mips, still. 1 array layer is the minimum we can have.
                MipLevels = 1,
                ArrayLayers = 1,
                Format = "rgba8"
            }
            -- StorageImages should also have a "Size" member, specifying how
            -- many texels of the format given in ImageOptions it will store
            Size = 1024
        },
        -- A UniformTexelBuffer is a read-only imageBuffer (called textureBuffer in GLSL),
        -- and requires a format and size as it's only members. It is a buffer with an
        -- attached BufferView object specifying the underlying format, so that formatted reads
        -- function.
        DemoUniformTexelBuffer = {
            Type = "UniformTexelBuffer",
            Format = "r11f_g11f_b10f",
            Size = 2048
        },
        -- A StorageTexelBuffer requires the same fields as a UniformTexelBuffer, but can be
        -- written to and read from by shaders.
        DemoStorageTexelBuffer = {
            Type = "StorageTexelBuffer",
            Format = "r32ui",
            Size = 4096
        },
        -- A storage buffer has a lot in common with a UniformBuffer, but can be written
        -- to (unlike a uniform buffer) by shaders. Note how a complex/composite type is
        -- added: it is assumed that the arrays ElementType 1. has had it's size added to 
        -- "ObjectSizes" and 2. has a definition either in an include .glsl file or inline 
        DemoStorageBuffer = {
            Type = "StorageBuffer",
            Members = {
                Data = { { 
                        Type = "Array",
                        ElementType = "AABB",
                        -- One of the benefits of Lua is that 
                        -- one can have functionally-defined size fields!
                        NumElements = get_num_aabbs()
                    }, 0 }
            }
        }
    },
    SubpassResources = {
        --[[
            Subpass inputs are "InputAttachments". Format is required,
            along with the input attachment index.
        ]]
        GBuffer = {
            Type = "InputAttachment",
            Format = "rgba10_a2",
            Index = 0
        },
        ColorBuffer = {
            Type = "InputAttachment",
            Format = "rgba8",
            Index = 1
        }
    }
}