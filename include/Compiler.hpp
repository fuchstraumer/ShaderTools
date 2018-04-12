#pragma once
#ifndef SHADER_TOOLS_COMPILER_HPP
#define SHADER_TOOLS_COMPILER_HPP
#include "CommonInclude.hpp"
#include "Shader.hpp"
namespace st {

    class ShaderCompilerImpl;

    class ShaderCompiler {
        ShaderCompiler(const ShaderCompiler&) = delete;
        ShaderCompiler& operator=(const ShaderCompiler&) = delete;
    public:
    
        ShaderCompiler();
        ~ShaderCompiler();
        ShaderCompiler(ShaderCompiler&& other) noexcept;
        ShaderCompiler& operator=(ShaderCompiler&& other) noexcept;

        Shader Compile(const std::string& name, const std::string& file_source, const VkShaderStageFlagBits stage);
        Shader Compile(const char* path_to_source, const VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);

        VkShaderStageFlags GetShaderStage(const char* path_to_source) const;
        bool HasShader(const Shader& shader_handle) const;
        std::vector<uint32_t> GetBinary(const Shader& shader_handle) const;
        void AddBinary(const char* path, const std::vector<uint32_t>& binary, const VkShaderStageFlagBits stage);

        void SaveShaderToAssemblyText(const Shader & shader_to_compile, const char * fname, const char* shader_name = "invalid_name");

        void SaveBinaryBackToText(const Shader& shader_to_save, const char* fname) const;
       
    private:
        std::unique_ptr<ShaderCompilerImpl> impl;
    };

}

#endif //!SHADER_TOOLS_COMPILER_HPP