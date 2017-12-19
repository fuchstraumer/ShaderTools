#include <iostream>
#include "ShaderGroup.hpp"
#include <filesystem>

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
    using namespace st;
    ShaderGroup group;
    for(const auto& path : args) {
        namespace fs = std::experimental::filesystem;
        fs::path curr(path);
        assert(curr.has_extension());
        VkShaderStageFlags stage = extension_stage_map.at(curr.extension().string());
        group.CompileAndAddShader(path, stage);
    }

    group.SaveToJSON("sets.json");
}