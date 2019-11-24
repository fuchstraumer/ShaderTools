#pragma once
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"
#include <string>
#include <vector>

namespace st
{

    ShaderStage WriteAndAddShaderSource(const std::string file_name, const std::string& file_contents, const VkShaderStageFlags stage);
    void WriteAndAddShaderBinary(const std::string base_name, const std::vector<uint32_t>& file_contents, const VkShaderStageFlags stage);
    std::string GetOutputDirectory();
    void SetOutputDirectory(const std::string& output_dir);

}
