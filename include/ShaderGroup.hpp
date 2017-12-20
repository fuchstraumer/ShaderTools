#pragma once 
#ifndef SHADER_TOOLS_GROUP_HPP
#define SHADER_TOOLS_GROUP_HPP
#include "Compiler.hpp"
#include "BindingGenerator.hpp"
namespace st {

    class ShaderGroup {
    public:
        ShaderGroup() = default;
        ~ShaderGroup() = default;

        void AddShader(const std::string& compiled_shader_path, const VkShaderStageFlags& stages);
        const std::vector<uint32_t>& CompileAndAddShader(const std::string& uncompiled_shader_path, const VkShaderStageFlags& stages);


    private:

        BindingGenerator bindings;
        ShaderCompiler compiler;

    };

}

#endif //!SHADER_TOOLS_GROUP_HPP