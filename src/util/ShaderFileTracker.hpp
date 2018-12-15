#pragma once
#ifndef ST_SHADER_FILE_TRACKER_HPP
#define ST_SHADER_FILE_TRACKER_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"
#include "ShaderPackBinary.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <experimental/filesystem>
#include <unordered_set>
#include <memory>

namespace st {

    class ResourceFile;

    struct ShaderFileTracker {
        ShaderFileTracker(const std::string& initial_directory = std::string{ "" });
        ~ShaderFileTracker();

        void DumpContentsToCacheDir();

        bool FindShaderBody(const ShaderStage& handle, std::string& dest_str);
        bool AddShaderBodyPath(const ShaderStage& handle, const std::string& shader_body_path);
        bool FindShaderBinary(const ShaderStage& handle, std::vector<uint32_t>& dest_binary_vector);
        bool FindResourceScript(const std::string& fname, const ResourceFile* dest_ptr);
        bool FindRecompiledShaderSource(const ShaderStage& handle, std::string& destination_str);
        bool FindAssemblyString(const ShaderStage& handle, std::string& destination_str);

        static ShaderFileTracker& GetFileTracker();

        std::experimental::filesystem::path cacheDir{ std::experimental::filesystem::temp_directory_path() };
        std::unordered_map<ShaderStage, std::experimental::filesystem::file_time_type> StageLastModificationTimes;
        std::unordered_map<ShaderStage, std::string> ShaderNames;
        std::unordered_map<ShaderStage, std::string> ShaderBodies;
        std::unordered_map<ShaderStage, std::string> RecompiledSourcesFromBinaries;
        std::unordered_map<ShaderStage, std::string> AssemblyStrings;
        std::unordered_map<ShaderStage, std::string> FullSourceStrings;
        std::unordered_map<ShaderStage, std::vector<uint32_t>> Binaries;
        std::unordered_map<ShaderStage, std::string> ShaderUsedResourceScript;
        std::unordered_multimap<ShaderStage, std::string> ShaderUsedResourceBlocks;
        std::unordered_map<std::string, std::unique_ptr<ResourceFile>> ResourceScripts;
        std::unordered_map<ShaderStage, std::experimental::filesystem::path> BodyPaths;
        std::unordered_map<ShaderStage, std::experimental::filesystem::path> BinaryPaths;
        std::unordered_map<std::string, size_t> ObjectSizes;
        std::unordered_map<std::string, std::unique_ptr<ShaderPackBinary, decltype(&DestroyShaderPackBinary)>> ShaderPackBinaries;
    };

}


#endif // !ST_SHADER_FILE_TRACKER_HPP
