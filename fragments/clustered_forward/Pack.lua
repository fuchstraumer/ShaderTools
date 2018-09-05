PackName = "ClusteredForwardShading"
ResourceFileName = "Resources.lua"
ShaderGroups = {
    DepthPrePass = { 
        Idx = 0,
        Shaders = { Vertex = "Simple.vert" },
        Tags = { "DepthOnly", "PrePass" },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    Lights = { 
        Idx = 1,
        Shaders = { Vertex = "Default.vert", Fragment = "Light.frag" },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    ComputeLightGrids = { 
        Idx = 2,
        Shaders = { Compute = "LightGrid.comp" },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    ComputeGridOffsets = { 
        Idx = 3,
        Shaders = { Compute = "GridOffsets.comp" },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack",
            "GL_EXT_control_flow_attributes"
        }
    },
    ComputeLightLists = { 
        Idx = 4,
        Shaders = { Compute = "LightList.comp" },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    },
    Clustered = { 
        Idx = 5,
        Shaders = { Vertex = "Default.vert", Fragment = "Clustered.frag" },
        Extensions = {
            "GL_ARB_separate_shader_objects",
            "GL_ARB_shading_language_420pack"
        }
    }
}
