#pragma once
#ifndef ST_SHADER_COMPILER_IMPL_HPP
#define ST_SHADER_COMPILER_IMPL_HPP
#include "common/ShaderStage.hpp"
#include "common/UtilityStructs.hpp"
#include "../../common/UtilityStructsInternal.hpp"

#include <string>

#include <vulkan/vulkan_core.h>
#include "shaderc/shaderc.hpp"

namespace st
{

    struct SessionImpl;

    class ShaderCompilerImpl
    {
        ShaderCompilerImpl(const ShaderCompilerImpl&) = delete;
        ShaderCompilerImpl& operator=(const ShaderCompilerImpl&) = delete;
    public:

        ShaderCompilerImpl(const ShaderCompilerOptions& options, SessionImpl* error_session) noexcept;
        ~ShaderCompilerImpl() noexcept;
        ShaderCompilerImpl(ShaderCompilerImpl&& other) noexcept = default;
        ShaderCompilerImpl& operator=(ShaderCompilerImpl&& other) noexcept = default;

        [[nodiscard]] shaderc::CompileOptions getCompilerOptions() const;
        [[nodiscard]] ShaderToolsErrorCode getShaderKind(const uint32_t& flags, shaderc_shader_kind& result) const;
        [[nodiscard]] ShaderToolsErrorCode prepareToCompile(const ShaderStage& handle, const char* path_to_src);
        [[nodiscard]] ShaderToolsErrorCode prepareToCompile(const ShaderStage& handle, std::string name, std::string src);
        [[nodiscard]] ShaderToolsErrorCode compile(const ShaderStage& handle, const shaderc_shader_kind& kind, std::string name, std::string src_str);
        [[nodiscard]] ShaderToolsErrorCode recompileBinaryToGLSL(const ShaderStage& handle, size_t* str_size, char* dest_str);
        [[nodiscard]] ShaderToolsErrorCode getBinaryAssemblyString(const ShaderStage& handle, size_t* str_size, char* dest_str);

        friend ShaderToolsErrorCode ST_API CompileStandaloneShader(
            ShaderStage& resultHandle,
            const char* shader_name,
            const VkShaderStageFlagBits shader_stage,
            const char* src_str, const size_t src_len);

        friend ShaderToolsErrorCode ST_API RetrieveCompiledStandaloneShader(
            const ShaderStage shader_handle,
            size_t* binary_sz,
            uint32_t* binary_dest);

        const ShaderCompilerOptions& compilerOptions;
        SessionImpl* errorSession;
    };

}

#endif // !ST_SHADER_COMPILER_IMPL_HPP
