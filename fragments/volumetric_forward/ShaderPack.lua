PackName = "VolumetricForwardCore"
ResourceFileName = "vfResources.lua"
ShaderGroups = {
    --[[
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
    ]]
    ComputeMortonCodes = {
        Idx = 0,
        Shaders = {
            Compute = "compute/ComputeMortonCodes.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    FindUniqueClusters = {
        Idx = 1,
        Shaders = {
            Compute = "compute/FindUniqueClusters.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    ReduceLights = {
        Idx = 2,
        Shaders = {
            Compute = "compute/ReduceLightsAABB.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    UpdateClusterIndirectArgs = {
        Idx = 3,
        Shaders = {
            Compute = "compute/UpdateClusterIndirectArgs.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    UpdateLights = {
        Idx = 4,
        Shaders = {
            Compute = "compute/UpdateLights.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    AssignLightsToClusters = {
        Idx = 5,
        Shaders = {
            Compute = "compute/AssignLightsToClusters.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack",
            "GL_EXT_control_flow_attributes"
        }
    },
    DepthPrePass = {
        Idx = 6,
        Shaders = {
            Vertex = "Default.vert",
            Fragment = "PrePass.frag"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }, 
        -- Use DepthOnly tag to force front-end to attach a depth stencil output
        -- These shaders probably don't write to the backbuffer for color, but
        -- we want to keep depth info.
        Tags = { "DepthOnly" }
    },
    ClusterSamples = {
        Idx = 7,
        Shaders = {
            Vertex = "Default.vert",
            Fragment = "ClusterSamples.frag"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }, 
        Tags = { "DepthOnlyAsInput" }
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
        }, 
        -- Draw pass reads depth but has no ability to write to it
        Tags = { "DepthOnlyAsInput" }
    }
}
