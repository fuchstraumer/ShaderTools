#include <map>
#include <iostream>
#include <vector>
#include "Compiler.hpp"
#include "BindingGenerator.hpp"

static const std::map<std::string, VkShaderStageFlags> extension_stage_map {
    { ".vert", VK_SHADER_STAGE_VERTEX_BIT },
    { ".frag", VK_SHADER_STAGE_FRAGMENT_BIT },
    { ".geom", VK_SHADER_STAGE_GEOMETRY_BIT },
    { ".teval", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT },
    { ".tcntl", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT },
    { ".comp", VK_SHADER_STAGE_COMPUTE_BIT }
};

int main(int argc, char* argv[]) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    std::vector<std::vector<uint32_t>> binaries;
    using namespace st;
    ShaderCompiler compiler;
    BindingGenerator parser;
    for(const auto& path : args) {
        Shader handle = compiler.Compile(path.c_str());
        {
            uint32_t size = 0;
            compiler.GetBinary(handle, &size, nullptr);
            std::vector<uint32_t> binary(size);
            compiler.GetBinary(handle, &size, binary.data());
            parser.ParseBinary(handle);
        }
    }
    parser.CollateBindings();
    parser.SaveToJSON("out.json");
}