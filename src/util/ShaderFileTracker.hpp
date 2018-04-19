#pragma once
#ifndef ST_SHADER_FILE_TRACKER_HPP
#define ST_SHADER_FILE_TRACKER_HPP
#include "common/CommonInclude.hpp"
#include "common/Shader.hpp"
#include "lua/ResourceFile.hpp"

#include <unordered_map>
#include <string>
#include <vector>
#include <experimental/filesystem>
namespace st {

    struct ShaderFileTracker {
        ShaderFileTracker();
        ~ShaderFileTracker();
        bool FindShaderBody(const Shader& handle, std::string& dest_str);
        bool AddShaderBodyPath(const Shader& handle, const std::string& shader_body_path);
        bool FindShaderBinary(const Shader& handle, std::vector<uint32_t>& dest_binary_vector);
        bool FindResourceScript(const std::string& fname, const ResourceFile* dest_ptr);
        bool ShaderSourceNewerThanBinary(const Shader& handle);

        std::unordered_map<Shader, std::string> Sources;
        std::unordered_map<Shader, std::vector<uint32_t>> Binaries;
        std::unordered_map<Shader, std::string> ShaderUsedResourceScript;
        std::unordered_map<std::string, std::unique_ptr<ResourceFile>> ResourceScripts;
        std::unordered_map<Shader, std::experimental::filesystem::path> SourcePaths;
        std::unordered_map<Shader, std::experimental::filesystem::path> BinaryPaths;

    private:
        bool attemptExecuteResourceScript(const std::string& full_file_path, const ResourceFile* dest_ptr);
    };

}


#endif // !ST_SHADER_FILE_TRACKER_HPP
