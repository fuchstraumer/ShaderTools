#include "common/Shader.hpp"
#include <string>
namespace st {

    inline uint64_t GetShaderHash(const char* shader_name, const VkShaderStageFlagBits stage) {
        const uint64_t base_hash = static_cast<uint64_t>(std::hash<std::string>()(std::string(shader_name)));
        const uint32_t stage_bits(stage);
        return (base_hash << 32) | (stage_bits);
    }

    Shader::Shader(const char* shader_name, const VkShaderStageFlagBits stages) : ID(GetShaderHash(shader_name, stages)) {}

    Shader::Shader(const Shader& other) noexcept : ID(other.ID) { }

    Shader & Shader::operator=(const Shader& other) noexcept {
        ID = other.ID;
        return *this;
    }

    Shader::Shader(Shader && other) noexcept : ID(std::move(other.ID)) {}

    Shader & Shader::operator=(Shader && other) noexcept {
        ID = std::move(other.ID);
        return *this;
    }

    VkShaderStageFlagBits Shader::GetStage() const noexcept {
        return VkShaderStageFlagBits((uint32_t)ID);
    }

}

