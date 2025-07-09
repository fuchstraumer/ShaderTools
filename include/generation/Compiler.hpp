#pragma once
#ifndef SHADER_TOOLS_COMPILER_HPP
#define SHADER_TOOLS_COMPILER_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"

namespace st
{

    class ShaderCompilerImpl;
    struct Session;

    /**
     * @brief Compiles shader stages from source code into SPIR-V assembly first, then SPIR-V binary. Provides access to both forms of the SPIR-V.
     * 
     * This class does just what it says on the tin, and very little more. The assembly is kept around as it can be useful for debugging edge case compile failures
     * or other issues (ask me how I know, *shudder*). The binary is the actual compiled data you will want to use, and translating from the assembly to the binary is
     * simple so that's why we still sorta do a "dual-stage" compiliation process.
     * 
     * @see ShaderFileTracker for where the data is actually stored and how it is tracked
     */
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
        [[nodiscard]] ShaderToolsErrorCode GetAssembly(const ShaderStage& shader_handle, size_t* assembly_size, char* dest_assembly_str) const;
        [[nodiscard]] ShaderToolsErrorCode RecompileBinaryToGLSL(const ShaderStage& shader_handle, size_t* recompiled_size, char* dest_glsl_str) const;
        void SaveBinaryToFile(const ShaderStage& handle, const char* fname);

    private:
        std::unique_ptr<ShaderCompilerImpl> impl;
    };

    /**
     * @brief Allows you to compile a standalone shader stage from given source code, without needing a shader pack, session, or anything else.
     * This function is intended to be used for quick prototyping or testing of shader code, where you don't need the full functionality of a shader pack or session. Or if you have things
     * like debug shaders, or external libraries that need you to compile their shader code.
     * @see RetrieveCompiledStandaloneShader for retrieving the compiled shader binary
     * @param resultHandle A reference to a ShaderStage object that you have constructed with the shader name and stage bits.
     * @param shader_name The name of the shader to compile, used for debugging and error messages.
     * @param shader_stage The Vulkan shader stage flags for the shader being compiled.
     * @param src_str The source code of the shader to compile, as a null-terminated string.
     * @param src_len The length of the source code string, in bytes.
     * @return ShaderToolsErrorCode indicating success or failure of the compilation process.
    */
    ShaderToolsErrorCode ST_API CompileStandaloneShader(
        ShaderStage& resultHandle,
        const char* shader_name,
        const VkShaderStageFlags shader_stage,
        const char* src_str, const size_t src_len);

    /**
     * @brief Retrieves the compiled shader binary for a standalone shader stage that was previously compiled with CompileStandaloneShader. Call twice, once with a null buffer and valid size_t 
     * to get the buffer size, then again with a valid buffer to get the actual binary data.
     * @param shader_handle The ShaderStage object that was used to compile the shader, containing the shader name and stage bits.
     * @param binary_sz A pointer to a size_t that will be set to the size of the binary data.
     * @param binary_dest A pointer to a buffer that will be filled with the compiled shader
     * @note Behavior is undefined if the binary_dest is not large enough to contain the binary data.
     * @return ShaderToolsErrorCode indicating success or failure of the operation.
     */
    ShaderToolsErrorCode ST_API RetrieveCompiledStandaloneShader(
        const ShaderStage shader_handle,
        size_t* binary_sz,
        uint32_t* binary_dest);

}

#endif //!SHADER_TOOLS_COMPILER_HPP
