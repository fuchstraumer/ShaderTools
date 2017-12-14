#pragma once
#ifndef SHADER_TOOLS_COMPILER_HPP
#define SHADER_TOOLS_COMPILER_HPP
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace st {

    class ShaderCompiler {
    public:
        ShaderCompiler() = default;
        ~ShaderCompiler() = default;
        
        const std::vector<uint32_t>& Compile(const std::string& path_to_source, const VkShaderStageFlags& stage);
        bool HasShader(const std::string& binary_path);
        const std::vector<uint32_t>& GetBinary(const std::string& binary_path);
        void AddBinary(const std::string& path, std::vector<uint32_t> binary_data);
        
    private:
        std::unordered_map<std::string, std::vector<uint32_t>> compiledShaders;
    };

}

#endif //!SHADER_TOOLS_COMPILER_HPP