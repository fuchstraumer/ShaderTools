#pragma once
#ifndef ST_SHADER_GROUP_IMPL_HPP
#define ST_SHADER_GROUP_IMPL_HPP
#include "common/ShaderStage.hpp"
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>

namespace st
{

    class ShaderGenerator;
    class ShaderCompiler;
    class ShaderReflector;
    struct yamlFile;
    struct Session;
    
    class ShaderGroupImpl
    {
        ShaderGroupImpl(const ShaderGroupImpl& other) = delete;
        ShaderGroupImpl& operator=(const ShaderGroupImpl& other) = delete;
    public:

        ShaderGroupImpl(const std::string& group_name, yamlFile* yaml_file, Session& error_session);
        ~ShaderGroupImpl();
        ShaderGroupImpl(ShaderGroupImpl&& other) noexcept;
        ShaderGroupImpl& operator=(ShaderGroupImpl&& other) noexcept;

        void addShaderStage(const ShaderStage& handle);

        std::string groupName;
        size_t idx;
        std::unordered_set<st::ShaderStage> stHandles{};
        std::unordered_map<st::ShaderStage, bool> optimizationEnabled;
        std::unique_ptr<ShaderReflector> reflector{ nullptr };
        yamlFile* rsrcFile{ nullptr };
        std::vector<std::string> tags;
        std::unordered_map<std::string, size_t> resourceGroupBindingIndices;
        std::filesystem::path resourceScriptPath;
    };

}

#endif // !ST_SHADER_GROUP_IMPL_HPP
