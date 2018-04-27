#pragma once
#ifndef ST_SHADER_FILE_TRACKER_HPP
#define ST_SHADER_FILE_TRACKER_HPP
#include "common/CommonInclude.hpp"
#include "common/Shader.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <experimental/filesystem>
#include <unordered_set>
namespace st {

    class ResourceFile;

    struct ShaderFileTracker {
        ShaderFileTracker(const std::string& initial_directory = std::string{ "" });
        ~ShaderFileTracker();

        void DumpContentsToCacheDir();

        bool RegisterShader(const Shader& handle);
        bool FindShaderBody(const Shader& handle, std::string& dest_str);
        bool AddShaderBodyPath(const Shader& handle, const std::string& shader_body_path);
        bool FindShaderBinary(const Shader& handle, std::vector<uint32_t>& dest_binary_vector);
        bool FindResourceScript(const std::string& fname, const ResourceFile* dest_ptr);
        bool FindRecompiledShaderSource(const Shader& handle, std::string& destination_str);
        bool FindAssemblyString(const Shader& handle, std::string& destination_str);

        static ShaderFileTracker& GetFileTracker();

        std::experimental::filesystem::path cacheDir{ std::experimental::filesystem::temp_directory_path() };
        std::unordered_map<Shader, std::string> ShaderNames;
        std::unordered_map<Shader, std::string> ShaderBodies;
        std::unordered_map<Shader, std::string> RecompiledSourcesFromBinaries;
        std::unordered_map<Shader, std::string> AssemblyStrings;
        std::unordered_map<Shader, std::string> FullSourceStrings;
        std::unordered_map<Shader, std::vector<uint32_t>> Binaries;
        std::unordered_map<Shader, std::string> ShaderUsedResourceScript;
        std::unordered_map<std::string, std::unique_ptr<ResourceFile>> ResourceScripts;
        std::unordered_map<Shader, std::experimental::filesystem::path> BodyPaths;
        std::unordered_map<Shader, std::experimental::filesystem::path> BinaryPaths;
        std::unordered_map<std::string, size_t> ObjectSizes;
    };

}


#endif // !ST_SHADER_FILE_TRACKER_HPP
