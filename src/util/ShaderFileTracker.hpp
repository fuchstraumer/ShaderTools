#pragma once
#ifndef ST_SHADER_FILE_TRACKER_HPP
#define ST_SHADER_FILE_TRACKER_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"
#include "common/ShaderToolsErrors.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_set>
#include <memory>
#include <mutex>

namespace st
{

    // Major TODO: This is totally not threadsafe. It completely nullifies any attempts to parallelize shader generation and the heavyweight
    // compilation process. We need to make this threadsafe, and we need to make it so that the file tracker can be used concurrently by
    // multiple threads. This will still generate some bottlenecks, and requires major changes to the usage of this object, but it's a start.
    // Should also assess how we can do this better, as there should be some way to track files on disk persistently just... less gross.

    void ST_API InitializeFileTracker(const char* cache_directory);
    void ST_API ClearProgramState();
    void ST_API DumpProgramStateToCache();

    struct ShaderFileTracker
    {
        ShaderFileTracker(const std::string& initial_directory = std::string{ "" });
        ~ShaderFileTracker();

        void ClearAllContainers();
        void DumpContentsToCacheDir();

        ShaderToolsErrorCode FindShaderBody(const ShaderStage& handle, std::string& dest_str);
        ShaderToolsErrorCode AddShaderBodyPath(const ShaderStage& handle, const std::string& shader_body_path);
        ShaderToolsErrorCode FindShaderBinary(const ShaderStage& handle, std::vector<uint32_t>& dest_binary_vector);
        bool FindRecompiledShaderSource(const ShaderStage& handle, std::string& destination_str);
        bool FindAssemblyString(const ShaderStage& handle, std::string& destination_str);
        std::string GetShaderName(const ShaderStage& handle);

        static ShaderFileTracker& GetFileTracker();

        std::recursive_mutex mapMutex;
        std::filesystem::path cacheDir{ std::filesystem::temp_directory_path() };
        std::unordered_map<ShaderStage, std::filesystem::file_time_type> StageLastModificationTimes;
        std::unordered_map<ShaderStage, std::string> ShaderBodies;
        std::unordered_map<ShaderStage, std::string> RecompiledSourcesFromBinaries;
        std::unordered_map<ShaderStage, std::string> AssemblyStrings;
        std::unordered_map<ShaderStage, std::string> FullSourceStrings;
        std::unordered_map<ShaderStage, std::vector<uint32_t>> Binaries;
        std::unordered_multimap<ShaderStage, std::string> ShaderUsedResourceBlocks;
        // first key is resource group, second map is stage and index of group in that stage
        std::unordered_map<std::string, std::unordered_map<ShaderStage, uint32_t>> ResourceGroupSetIndexMaps;
        std::unordered_map<ShaderStage, bool> StageOptimizationDisabled;
        std::unordered_map<ShaderStage, std::filesystem::path> BodyPaths;
        std::unordered_map<ShaderStage, std::filesystem::path> BinaryPaths;
    };

}


#endif // !ST_SHADER_FILE_TRACKER_HPP
