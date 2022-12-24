#pragma once
#ifndef ST_SHADER_HPP
#define ST_SHADER_HPP
#include "CommonInclude.hpp"

namespace st
{

    struct ST_API ShaderStage
    {
        ShaderStage(const char* shader_name, const VkShaderStageFlags stages);
        ShaderStage(uint32_t hash, uint32_t stageBits) noexcept;
        ShaderStage(const ShaderStage& other) noexcept = default;
        ShaderStage& operator=(const ShaderStage& other) noexcept = default;
        ShaderStage(ShaderStage&& other) noexcept = default;
        ShaderStage& operator=(ShaderStage&& other) noexcept = default;
        uint32_t hash;
        uint32_t stageBits;
        bool operator==(const ShaderStage& other) const noexcept;
        bool operator<(const ShaderStage& other) const noexcept;
    };

}

namespace std
{

    template<>
    struct hash<st::ShaderStage>
    {
        size_t operator()(const st::ShaderStage& shader) const noexcept
        {
            return std::hash<uint64_t>()((static_cast<uint64_t>(shader.hash) << 32) | shader.stageBits);
        }
    };

}

#endif //!ST_SHADER_HPP
