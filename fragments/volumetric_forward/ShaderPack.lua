PackName = "VolumetricForwardCore"
ResourceFileName = "vfResources.lua"
ShaderGroups = {
    AssignLightsToClusters = {
        Compute = "compute/AssignLightsToClustersBVH.comp"
    },
    BuildBVH = {
        Compute = "compute/BuildBVH.comp"
    },
    ComputeMortonCodes = {
        Compute = "compute/ComputeMortonCodes.comp"
    },
    FindUniqueClusters = {
        Compute = "compute/FindUniqueClusters.comp"
    },
    ReduceLights = {
        Compute = "compute/ReduceLightsAABB.comp"
    },
    UpdateClusterIndirectArgs = {
        Compute = "compute/UpdateClusterIndirectArgs.comp"
    },
    UpdateLights = {
        Compute = "compute/UpdateLights.comp"
    },
    DepthPrePass = {
        Vertex = "Default.vert",
        Fragment = "ClusterSamples.frag"
    },
    DrawPass = {
        Vertex = "Default.vert",
        Fragment = "Clustered.frag"
    }
}