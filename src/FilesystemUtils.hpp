#pragma once
#include "CommonInclude.hpp"
#include "ShaderStructs.hpp"
#include <string>
#include <experimental/filesystem>

namespace st {

    inline uint32_t GetShaderHash(const std::experimental::filesystem::path& base_file_path,
        const VkShaderStageFlagBits stage) {
        namespace fs = std::experimental::filesystem;
        const uint32_t base_hash = static_cast<uint32_t>(std::hash<std::string>()(fs::absolute(base_file_path).string()));
        const uint16_t stage_bits(stage);
        return (base_hash << 16) | (stage_bits);
    }

    static ShaderHandle WriteAndAddShaderSource(const std::string file_name, const std::string& file_contents, const VkShaderStageFlags stage);
    static void WriteAndAddShaderBinary(const std::string base_name, const std::vector<uint32_t>& file_contents, const VkShaderStageFlags stage);

}