PackName = "VolumetricForwardCore"
ResourceFileName = "vfResources.lua"
ShaderGroups = {
    UpdateLights = {
        Idx = 0,
        Shaders = {
            Compute = "compute/UpdateLights.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    ReduceLights = {
        Idx = 1,
        Shaders = {
            Compute = "compute/ReduceLightsAABB.comp"
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
    RadixSort = {
        Idx = 3,
        Shaders = {
            Compute = "compute/RadixSort.comp"
        }
    },
    MergeSort = {
        Idx = 4,
        Shaders = {
            Compute = "compute/MergeSort.comp"
        }
    },
    BuildBVH = {
        Idx = 5,
        Shaders = {
            Compute = "compute/BuildBVH.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
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
    FindUniqueClusters = {
        Idx = 8,
        Shaders = {
            Compute = "compute/FindUniqueClusters.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    UpdateClusterIndirectArgs = {
        Idx = 9,
        Shaders = {
            Compute = "compute/UpdateClusterIndirectArgs.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    AssignLightsToClusters = {
        Idx = 10,
        Shaders = {
            Compute = "compute/AssignLightsToClustersBVH.comp"
        },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack",
            "GL_EXT_control_flow_attributes"
        }
    },
    DrawPass = {
        Idx = 11,
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
    },
    -- Run before everything else, but only once per window resizing
    -- So I'm putting it down here
    ComputeClusterAABBs = {
        Idx = 12,
        Shaders = {
            Compute = "compute/ComputeClusterAABBs.comp"
        }
    },
    DebugClusters = {
        Idx = 13,
        Shaders = {
            Vertex = "debug/DebugClusters.vert",
            Geometry = "debug/DebugClusters.geom",
            Fragment = "debug/DebugClusters.frag"
        }
    }
}
