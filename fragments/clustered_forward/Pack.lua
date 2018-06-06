PackName = "ClusteredForwardShading"
ResourceFileName = "Resources.lua"
ShaderGroups = {
    { 0, DepthPrePass = {
        Vertex = "Simple.vert"
    } },
    { 1, Lights = {
        Vertex = "Default.vert",
        Fragment = "Light.frag"
    } },
    { 2, ComputeLightGrids = {
        Compute = "LightGrid.comp"
    } },
    { 3, ComputeGridOffsets = {
        Compute = "GridOffsets.comp"
    } },
    { 4, ComputeLightLists = {
        Compute = "LightList.comp"
    } },
    { 5, Clustered = {
        Vertex = "Default.vert",
        Fragment = "Clustered.frag"
    } }
}