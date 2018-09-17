PackName = "VolumetricForwardCore"
ResourceFileName = "vfResources.lua"
ShaderGroups = {
    AssignLightsToClusters = {
        Idx = 0,
        Shaders = { Compute = "compute/AssignLightsToClustersBVH.comp" },

    },
    BuildBVH = {
        Idx = 1,
        Shaders = {
            Compute = "compute/BuildBVH.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    ComputeMortonCodes = {
        Idx = 2,
        Shaders = {
            Compute = "compute/ComputeMortonCodes.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    FindUniqueClusters = {
        Idx = 3,
        Shaders = {
            Compute = "compute/FindUniqueClusters.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    ReduceLights = {
        Idx = 4,
        Shaders = {
            Compute = "compute/ReduceLightsAABB.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    UpdateClusterIndirectArgs = {
        Idx = 5,
        Shaders = {
            Compute = "compute/UpdateClusterIndirectArgs.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    UpdateLights = {
        Idx = 6,
        Shaders = {
            Compute = "compute/UpdateLights.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    DepthPrePass = {
        Idx = 7,
        Shaders = {
            Vertex = "Default.vert",
            Fragment = "ClusterSamples.frag"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    DrawPass = {
        Idx = 8,
        Shaders = {
            Vertex = "Default.vert",
            Fragment = "Clustered.frag"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    }
}