PackName = "VolumetricForwardCore"
ResourceFileName = "vfResources.lua"
ShaderGroups = {
    AssignLightsToClusters = {
        Shader = "compute/AssignLightsToClustersBVH.comp"
    },
    BuildBVH = {
        Shader = "compute/BuildBVH.comp"
    },
    ComputeMortonCodes = {
        Shader = "compute/ComputeMortonCodes.comp"
    },
    FindUniqueClusters = {
        Shader = "compute/FindUniqueClusters.comp"
    },
    ReduceLights = {
        Shader = "compute/ReduceLightsAABB.comp"
    },
    UpdateClusterIndirectArgs = {
        Shader = "compute/UpdateClusterIndirectArgs.comp"
    },
    UpdateLights = {
        Shader = "compute/UpdateLights.comp"
    },
    DepthPrePass = {
        NumShaders = 2,
        Shaders = {
            "Default.vert",
            "ClusterSamples.frag"
        }
    },
    DrawPass = {
        NumShaders = 2,
        Shaders = {
            "Default.vert",
            "Clustered.frag"
        }
    }
}