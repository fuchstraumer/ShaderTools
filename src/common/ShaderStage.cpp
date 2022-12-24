#include "common/ShaderStage.hpp"
#include <string>

namespace st
{

    uint32_t GetShaderHash(const char* shader_name) noexcept
    {
       return static_cast<uint32_t>(std::hash<std::string>()(std::string(shader_name)));
    }

    ShaderStage::ShaderStage(const char* shader_name, const VkShaderStageFlags stages) : hash(GetShaderHash(shader_name)), stageBits(static_cast<uint32_t>(stages)) {}

    ShaderStage::ShaderStage(uint32_t _hash, uint32_t _stageBits) noexcept : hash(_hash), stageBits(_stageBits)
    {
    }

    bool ShaderStage::operator==(const ShaderStage & other) const noexcept
    {
        return (hash == other.hash) && (stageBits == other.stageBits);
    }

    bool ShaderStage::operator<(const ShaderStage & other) const noexcept
    {
        return stageBits <= other.stageBits;
    }

}
