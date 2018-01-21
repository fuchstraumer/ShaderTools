#include <map>
#include <iostream>
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
        if (!compiler.Compile(path.c_str())) {
            std::cerr << "Shader compiliation failed.\n";
            throw std::runtime_error("Failed to compile shader.");
        }
        else {
            uint32_t size = 0;
            compiler.GetBinary(path.c_str(), &size, nullptr);
            std::vector<uint32_t> binary(size);
            compiler.GetBinary(path.c_str(), &size, binary.data());
            parser.ParseBinary(size, binary.data(), compiler.GetShaderStage(path.c_str()));
        }
    }
    parser.CollateBindings();
    parser.SaveToJSON("out.json");
}