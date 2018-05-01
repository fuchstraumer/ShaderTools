#pragma once
#ifndef SHADER_TOOLS_COMPILER_HPP
#define SHADER_TOOLS_COMPILER_HPP
#include "common/CommonInclude.hpp"
#include "common/Shader.hpp"
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

        void Compile(const Shader& handle, const char* shader_name, const char* src_str, const size_t src_len);
        void Compile(const Shader& handle, const char* path_to_source);

        void GetBinary(const Shader& shader_handle, size_t* binary_sz, uint32_t* binary_dest_ptr) const;
        void GetAssembly(const Shader& shader_handle, size_t* assembly_size, char* dest_assembly_str) const;
        void RecompileBinaryToGLSL(const Shader& shader_handle, size_t* recompiled_size, char* dest_glsl_str) const;

    private:
        std::unique_ptr<ShaderCompilerImpl> impl;
    };

}

#endif //!SHADER_TOOLS_COMPILER_HPP