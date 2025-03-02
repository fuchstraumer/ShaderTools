#include "common/ShaderStage.hpp"
#include <string_view>

namespace st
{

    uint32_t GetShaderHash(const char* shader_name) noexcept
    {
       return static_cast<uint32_t>(std::hash<std::string_view>()(std::string_view(shader_name)));
    }

    ShaderStage::ShaderStage(const char* shader_name, const VkShaderStageFlags stages) : hash(GetShaderHash(shader_name)), stageBits(static_cast<uint32_t>(stages))
    {}

    constexpr ShaderStage::ShaderStage(uint32_t _hash, uint32_t _stageBits) noexcept : hash(_hash), stageBits(_stageBits)
    {}

    constexpr bool ShaderStage::operator==(const ShaderStage & other) const noexcept
    {
        return (hash == other.hash) && (stageBits == other.stageBits);
    }

    constexpr bool ShaderStage::operator<(const ShaderStage & other) const noexcept
    {
        return stageBits <= other.stageBits;
    }

}
