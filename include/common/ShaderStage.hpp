#pragma once
#ifndef ST_SHADER_HPP
#define ST_SHADER_HPP
#include "CommonInclude.hpp"

namespace st
{

    struct ST_API ShaderStage
    {
        ShaderStage(const char* shader_name, const VkShaderStageFlags stages);
        explicit ShaderStage(uint64_t id_val) noexcept;
        ShaderStage(const ShaderStage& other) noexcept;
        ShaderStage& operator=(const ShaderStage& other) noexcept;
        ShaderStage(ShaderStage&& other) noexcept;
        ShaderStage& operator=(ShaderStage&& other) noexcept;
        uint64_t ID;
        VkShaderStageFlagBits GetStage() const noexcept;
        bool operator==(const ShaderStage& other) const noexcept;
        bool operator<(const ShaderStage& other) const noexcept;
    };

}

namespace std
{

    template<>
    struct hash<st::ShaderStage>
    {
        constexpr size_t operator()(const st::ShaderStage& shader) const noexcept
        {
            return std::hash<uint64_t>()(shader.ID);
        }
    };

}

#endif //!ST_SHADER_HPP
