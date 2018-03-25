#pragma once
#ifndef ST_SHADER_HPP
#define ST_SHADER_HPP
#include "CommonInclude.hpp"

namespace st {

    struct ST_API Shader {
        Shader(const char* shader_path, const VkShaderStageFlagBits stages);
        uint64_t ID;
        VkShaderStageFlagBits GetStage() const noexcept;
        bool operator==(const Shader& other) const noexcept {
            return ID == other.ID;
        }
    };

}

namespace std {

    template<>
    struct hash<st::Shader> {
        size_t operator()(const st::Shader& shader) const {
            return std::hash<uint64_t>()(shader.ID);
        }
    };

}

#endif //!ST_SHADER_HPP