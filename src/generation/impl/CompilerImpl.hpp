#pragma once
#ifndef ST_SHADER_COMPILER_IMPL_HPP
#define ST_SHADER_COMPILER_IMPL_HPP
#include "common/ShaderStage.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include "shaderc/shaderc.hpp"

namespace st {

    class ShaderCompilerImpl {
        ShaderCompilerImpl(const ShaderCompilerImpl&) = delete;
        ShaderCompilerImpl& operator=(const ShaderCompilerImpl&) = delete;
    public:

        ShaderCompilerImpl() = default;
        ~ShaderCompilerImpl() = default;
        ShaderCompilerImpl(ShaderCompilerImpl&& other) noexcept = default;
        ShaderCompilerImpl& operator=(ShaderCompilerImpl&& other) noexcept = default;

        shaderc::CompileOptions getCompilerOptions() const;
        shaderc_shader_kind getShaderKind(const VkShaderStageFlagBits & flags) const;
        void prepareToCompile(const ShaderStage& handle, const char* path_to_src);
        void prepareToCompile(const ShaderStage& handle, const std::string& name, const std::string& src);
        void compile(const ShaderStage& handle, const shaderc_shader_kind& kind, const std::string& name, const std::string& src_str);
        void recompileBinaryToGLSL(const ShaderStage& handle, size_t* str_size, char* dest_str);
        void getBinaryAssemblyString(const ShaderStage & handle, size_t * str_size, char * dest_str);

    };

}

#endif // !ST_SHADER_COMPILER_IMPL_HPP
