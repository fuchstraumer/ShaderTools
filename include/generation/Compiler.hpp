#pragma once
#ifndef SHADER_TOOLS_COMPILER_HPP
#define SHADER_TOOLS_COMPILER_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"

namespace st
{

    class ShaderCompilerImpl;
    struct Session;

    class ST_API ShaderCompiler
    {
        ShaderCompiler(const ShaderCompiler&) = delete;
        ShaderCompiler& operator=(const ShaderCompiler&) = delete;
    public:

        ShaderCompiler(Session& error_session);
        ~ShaderCompiler();
        ShaderCompiler(ShaderCompiler&& other) noexcept;
        ShaderCompiler& operator=(ShaderCompiler&& other) noexcept;

        ShaderToolsErrorCode Compile(const ShaderStage& handle, const char* shader_name, const char* src_str, const size_t src_len);
        ShaderToolsErrorCode Compile(const ShaderStage& handle, const char* path_to_source);

        void GetBinary(const ShaderStage& shader_handle, size_t* binary_sz, uint32_t* binary_dest_ptr) const;
        void GetAssembly(const ShaderStage& shader_handle, size_t* assembly_size, char* dest_assembly_str) const;
        void RecompileBinaryToGLSL(const ShaderStage& shader_handle, size_t* recompiled_size, char* dest_glsl_str) const;
        void SaveBinaryToFile(const ShaderStage& handle, const char* fname);

    private:
        std::unique_ptr<ShaderCompilerImpl> impl;
    };

    ShaderToolsErrorCode ST_API CompileStandaloneShader(
        ShaderStage& resultHandle,
        const char* shader_name,
        const VkShaderStageFlags shader_stage,
        const char* src_str, const size_t src_len);
        
    ShaderToolsErrorCode ST_API RetrieveCompiledStandaloneShader(
        const ShaderStage shader_handle,
        size_t* binary_sz,
        uint32_t* binary_dest);

}

#endif //!SHADER_TOOLS_COMPILER_HPP
