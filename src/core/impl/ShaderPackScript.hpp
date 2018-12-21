#pragma once
#ifndef ST_SHADER_PACK_SCRIPT_HPP
#define ST_SHADER_PACK_SCRIPT_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"
#include "../../lua/LuaEnvironment.hpp"
#include <set>
#include <map>
#include <string>
#include <unordered_map>

namespace st {

    struct ShaderPackScript {

        ShaderPackScript(const char* fname);
        ~ShaderPackScript();

        std::string PackName;
        std::string ResourceFileName;
        std::unordered_map<std::string, size_t> GroupIndices;
        std::unordered_map<std::string, ShaderStage> Stages;
        std::unordered_map<std::string, std::set<ShaderStage>> ShaderGroups;
        std::unordered_map<std::string, std::vector<std::string>> GroupTags;
        std::unordered_map<ShaderStage, std::vector<std::string>> StageExtensions;
        std::unique_ptr<LuaEnvironment> environment;

        void parseScript();
    };

}

#endif //!ST_SHADER_PACK_SCRIPT_HPP
