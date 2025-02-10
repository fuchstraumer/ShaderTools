#pragma once
#ifndef ST_PARSER_YAML_FILE_HPP
#define ST_PARSER_YAML_FILE_HPP
#include "resources/ShaderResource.hpp"
#include "common/ShaderStage.hpp"
#include "common/stSession.hpp"
#include "common/UtilityStructs.hpp"
#include <memory>
#include <unordered_map>
#include <set>
#include <vector>
#include <string>


namespace st
{

    struct yamlFileImpl;

    struct yamlFile
    {

        yamlFile(const char* fname, Session& session);
        ~yamlFile();

        yamlFile(const yamlFile&) = delete;
        yamlFile& operator=(const yamlFile&) = delete;

        yamlFile(yamlFile&& other) noexcept;
        yamlFile& operator=(yamlFile&& other) noexcept;

        ShaderResource* FindResource(const std::string& name);
        std::unordered_map<std::string, ShaderStage> stages;
        std::unordered_map<std::string, std::set<ShaderStage>> shaderGroups;
        std::unordered_map<std::string, std::vector<std::string>> groupTags;
        std::unordered_map<ShaderStage, std::vector<std::string>> stageExtensions;
        std::unordered_map<std::string, std::vector<ShaderResource>> resourceGroups;
        ShaderCompilerOptions compilerOptions;
        std::string packName;

    private:
        ShaderToolsErrorCode parseResources(Session& session);
        ShaderToolsErrorCode parseGroups(Session& session);
        ShaderToolsErrorCode parseCompilerOptions(Session& session);
        void sortResourcesAndSetBindingIndices();
        std::unique_ptr<yamlFileImpl> impl;
    };

}

#endif // !ST_PARSER_YAML_FILE_HPP
