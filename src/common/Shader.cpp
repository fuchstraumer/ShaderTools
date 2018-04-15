#include "common/Shader.hpp"
#include <experimental/filesystem>

namespace st {

    namespace fs = std::experimental::filesystem;
    inline uint64_t GetShaderHash(const fs::path& base_file_path, const VkShaderStageFlagBits stage) {
        namespace fs = std::experimental::filesystem;
        const uint64_t base_hash = static_cast<uint64_t>(std::hash<std::string>()(fs::absolute(base_file_path).string()));
        const uint32_t stage_bits(stage);
        return (base_hash << 32) | (stage_bits);
    }

    Shader::Shader(const char* file_path, const VkShaderStageFlagBits stages) : ID(GetShaderHash(fs::path(file_path), stages)) {}

    VkShaderStageFlagBits Shader::GetStage() const noexcept {
        return VkShaderStageFlagBits((uint32_t)ID);
    }

}

