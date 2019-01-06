#pragma once
#ifndef ST_PARSER_YAML_FILE_HPP
#define ST_PARSER_YAML_FILE_HPP
#include "core/ShaderResource.hpp"
#include "common/ShaderStage.hpp"
#include <memory>
#include <unordered_map>
#include <set>
#include <vector>

namespace st {

    struct yamlFileImpl;

    struct yamlFile {

        yamlFile(const char* fname);
        ~yamlFile();

        std::unordered_map<std::string, ShaderStage> stages;
        std::unordered_map<std::string, std::set<ShaderStage>> groups;
        std::unordered_map<std::string, std::vector<std::string>> groupTags;
        std::unordered_map<ShaderStage, std::vector<std::string>> stageExtensions;
        std::unordered_map<std::string, std::vector<ShaderResource>> resourceGroups;

    private:
        std::unique_ptr<yamlFileImpl> impl;
    };

}

#endif // !ST_PARSER_YAML_FILE_HPP