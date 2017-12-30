#pragma once
#ifndef SHADER_TOOLS_COMPILER_HPP
#define SHADER_TOOLS_COMPILER_HPP
#include <vulkan/vulkan.h>
#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <filesystem>

namespace st {

    class ShaderCompiler {
    public:
        ShaderCompiler() = default;
        ~ShaderCompiler() = default;
        
        const std::vector<uint32_t>& Compile(const std::string& path_to_source, VkShaderStageFlags stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
        bool HasShader(const std::string& binary_path) const;
        const std::vector<uint32_t>& GetBinary(const std::string& binary_path) const;
        void AddBinary(const std::string& path, std::vector<uint32_t> binary_data);
        
        static std::string GetPreferredDirectory();
        static void SetPreferredDirectory(const std::string& new_dir);
        
    private:
        static std::string preferredShaderDirectory;
        static bool saveCompiledBinaries;
        std::map<std::experimental::filesystem::path, std::vector<uint32_t>> compiledShaders;
        void saveShaderToFile(const std::experimental::filesystem::path& source_path);
        void saveBinary(const std::experimental::filesystem::path& source_path, const std::experimental::filesystem::path& path_to_save_to);
        void loadAllSavedShaders();
        bool shaderSourceNewerThanBinary(const std::experimental::filesystem::path& source, const std::experimental::filesystem::path& binary);
    };

}

#endif //!SHADER_TOOLS_COMPILER_HPP