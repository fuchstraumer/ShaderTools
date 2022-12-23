#pragma once
#ifndef ST_PARSER_YAML_FILE_HPP
#define ST_PARSER_YAML_FILE_HPP
#include "resources/ShaderResource.hpp"
#include "common/ShaderStage.hpp"
#include <memory>
#include <unordered_map>
#include <set>
#include <vector>
#ifdef FindResource
#undef FindResource
#endif

namespace st {

    struct yamlFileImpl;

    struct yamlFile
    {

        yamlFile(const char* fname);
        ~yamlFile();

        ShaderResource* FindResource(const std::string& name);
        std::unordered_map<std::string, ShaderStage> stages;
        std::unordered_map<std::string, std::set<ShaderStage>> shaderGroups;
        std::unordered_map<std::string, std::vector<std::string>> groupTags;
        std::unordered_map<ShaderStage, std::vector<std::string>> stageExtensions;
        std::unordered_map<std::string, std::vector<ShaderResource>> resourceGroups;
        std::string packName;

    private:
        void parseResources();
        void parseGroups();
        void sortResourcesAndSetBindingIndices();
        std::unique_ptr<yamlFileImpl> impl;
    };

}

#endif // !ST_PARSER_YAML_FILE_HPP
