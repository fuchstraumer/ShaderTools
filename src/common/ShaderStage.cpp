#include "common/Shader.hpp"
#include <string>
namespace st {

    inline uint64_t GetShaderHash(const char* shader_name, const VkShaderStageFlagBits stage) {
        const uint64_t base_hash = static_cast<uint64_t>(std::hash<std::string>()(std::string(shader_name)));
        const uint32_t stage_bits(stage);
        return (base_hash << 32) | (stage_bits);
    }

    ShaderStage::ShaderStage(const char* shader_name, const VkShaderStageFlagBits stages) : ID(GetShaderHash(shader_name, stages)) {}

    ShaderStage::ShaderStage(const ShaderStage& other) noexcept : ID(other.ID) { }

    ShaderStage & ShaderStage::operator=(const ShaderStage& other) noexcept {
        ID = other.ID;
        return *this;
    }

    ShaderStage::ShaderStage(ShaderStage && other) noexcept : ID(std::move(other.ID)) {}

    ShaderStage & ShaderStage::operator=(ShaderStage && other) noexcept {
        ID = std::move(other.ID);
        return *this;
    }

    VkShaderStageFlagBits ShaderStage::GetStage() const noexcept {
        return VkShaderStageFlagBits((uint32_t)ID);
    }

}
