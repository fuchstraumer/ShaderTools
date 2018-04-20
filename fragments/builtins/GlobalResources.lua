Resources = {
    GlobalResources = {
        matrices = {
            Type = "UniformBuffer",
            Members = {
                model = "mat4",
                view = "mat4",
                projection = "mat4",
                normal = "mat4"
            }
        },
        globals = {
            Type = "UniformBuffer",
            Members = {
                viewPosition = "vec4",
                mousePosition = "vec2",
                windowSize = "vec2",
                depthRange = "vec2",
                frame = "uint"
            }
        },
        lightingData = {
            Type = "UniformBuffer",
            Members = {
                Exposure = "float",
                Gamma = "float"
            }
        }
    }
}