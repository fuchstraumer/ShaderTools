#pragma once
#ifndef SHADER_TOOLS_COMPILER_HPP
#define SHADER_TOOLS_COMPILER_HPP
#include "CommonInclude.hpp"
#include "Shader.hpp"
namespace st {

    class ShaderCompilerImpl;

    class ST_API ShaderCompiler {
        ShaderCompiler(const ShaderCompiler&) = delete;
        ShaderCompiler& operator=(const ShaderCompiler&) = delete;
    public:
    
        ShaderCompiler();
        ~ShaderCompiler();
        ShaderCompiler(ShaderCompiler&& other) noexcept;
        ShaderCompiler& operator=(ShaderCompiler&& other) noexcept;

        Shader Compile(const char* name, const char* file_src, const size_t len, const VkShaderStageFlagBits stage);
        Shader Compile(const char* path_to_source, const VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
        VkShaderStageFlags GetShaderStage(const char* path_to_source) const;
        bool HasShader(const Shader& shader_handle) const;
        void GetBinary(const Shader& shader_handle, uint32_t* binary_size, uint32_t* binary = nullptr) const;
        void AddBinary(const char* path, const uint32_t binary_size, const uint32_t* binary_src, const VkShaderStageFlagBits stage);
       
    private:
        std::unique_ptr<ShaderCompilerImpl> impl;
    };

}

#endif //!SHADER_TOOLS_COMPILER_HPP