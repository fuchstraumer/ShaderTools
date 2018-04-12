#pragma once
#include "CommonInclude.hpp"
#include <string>
#include <experimental/filesystem>
#include "Shader.hpp"

namespace st {

    Shader WriteAndAddShaderSource(const std::string file_name, const std::string& file_contents, const VkShaderStageFlagBits stage);
    void WriteAndAddShaderBinary(const std::string base_name, const std::vector<uint32_t>& file_contents, const VkShaderStageFlagBits stage);
    bool ShaderSourceNewerThanBinary(const Shader& source, const Shader& binary);
    std::string GetOutputDirectory();
    void SetOutputDirectory(const std::string& output_dir);

}