#pragma once
#ifndef SHADER_TOOLS_COMPILER_HPP
#define SHADER_TOOLS_COMPILER_HPP
#include "CommonInclude.hpp"

namespace st {

    class ShaderCompilerImpl;

    class ST_API ShaderCompiler {
    public:
        ShaderCompiler() = default;
        ~ShaderCompiler() = default;

        bool Compile(const char* path_to_source, VkShaderStageFlags stage);
        bool HasShader(const char* binary_path) const;
        void GetBinary(const char* binary_path, uint32_t* binary_size, uint32_t* binary = nullptr) const;
        void AddBinary(const char* path, uint32_t binary_size, uint32_t* binary_src);
        
        static const char* GetPreferredDirectory();
        static void SetPreferredDirectory(const char* directory);
        
    private:
        std::unique_ptr<ShaderCompilerImpl> impl;
    };

}

#endif //!SHADER_TOOLS_COMPILER_HPP