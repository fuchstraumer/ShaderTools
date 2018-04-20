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
        ShaderFileTracker(const std::string& initial_directory = std::string{ "" });
        ~ShaderFileTracker();
        bool FindShaderBody(const Shader& handle, std::string& dest_str);
        bool AddShaderBodyPath(const Shader& handle, const std::string& shader_body_path);
        bool FindShaderBinary(const Shader& handle, std::vector<uint32_t>& dest_binary_vector);
        bool FindResourceScript(const std::string& fname, const ResourceFile* dest_ptr);

        std::experimental::filesystem::path cacheDir{ std::experimental::filesystem::temp_directory_path() };
        std::unordered_map<Shader, std::string> ShaderNames;
        std::unordered_map<Shader, std::string> ShaderBodies;
        std::unordered_map<Shader, std::vector<uint32_t>> Binaries;
        std::unordered_map<Shader, std::string> ShaderUsedResourceScript;
        std::unordered_map<std::string, std::unique_ptr<ResourceFile>> ResourceScripts;
        std::unordered_map<Shader, std::experimental::filesystem::path> BodyPaths;
        std::unordered_map<Shader, std::experimental::filesystem::path> BinaryPaths;

    };

}


#endif // !ST_SHADER_FILE_TRACKER_HPP
