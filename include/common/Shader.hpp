#pragma once
#ifndef ST_SHADER_HPP
#define ST_SHADER_HPP
#include "CommonInclude.hpp"

namespace st {

    class ShaderCompiler;


    class Shader {
    public:

        Shader(const char* shader_name, const VkShaderStageFlagBits stages);

        void addBody(const char* fname);
        void addBody(const std::string& source);
        VkShaderStageFlagBits getStage() const noexcept;
        void compile();

        uint64_t ID;
        std::string sourceCode{ "" };
        std::vector<uint32_t> binaryData;
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