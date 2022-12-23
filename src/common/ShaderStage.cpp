#include "common/ShaderStage.hpp"
#include <string>

namespace st
{

    constexpr uint64_t GetShaderHash(const char* shader_name, const VkShaderStageFlags stage) noexcept
    {
        const uint64_t base_hash = static_cast<uint64_t>(std::hash<std::string>()(std::string(shader_name)));
        return (base_hash << 8) | (stage);
    }

    ShaderStage::ShaderStage(const char* shader_name, const VkShaderStageFlags stages) : ID(GetShaderHash(shader_name, stages)) {}

    ShaderStage::ShaderStage(uint64_t id_val) noexcept : ID(std::move(id_val)) {}

    ShaderStage::ShaderStage(const ShaderStage& other) noexcept : ID(other.ID) {}

    ShaderStage & ShaderStage::operator=(const ShaderStage& other) noexcept
    {
        ID = other.ID;
        return *this;
    }

    ShaderStage::ShaderStage(ShaderStage && other) noexcept : ID(std::move(other.ID)) {}

    ShaderStage & ShaderStage::operator=(ShaderStage && other) noexcept
    {
        ID = std::move(other.ID);
        return *this;
    }

    VkShaderStageFlagBits ShaderStage::GetStage() const noexcept
    {
        return VkShaderStageFlagBits((uint32_t)ID);
    }

    bool ShaderStage::operator==(const ShaderStage & other) const noexcept
    {
        return ID == other.ID;
    }

    bool ShaderStage::operator<(const ShaderStage & other) const noexcept
    {
        return GetStage() <= other.GetStage();
    }

}
