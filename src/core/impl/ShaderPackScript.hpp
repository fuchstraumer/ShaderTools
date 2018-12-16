#pragma once
#ifndef ST_SHADER_PACK_SCRIPT_HPP
#define ST_SHADER_PACK_SCRIPT_HPP
#include "common/CommonInclude.hpp"
#include "../../lua/LuaEnvironment.hpp"
#include <map>
#include <string>
#include <unordered_map>

namespace st {

    struct ShaderPackScript {

        ShaderPackScript(const char* fname);
        ~shader_pack_file_t() {};

        std::string PackName;
        std::string ResourceFileName;
        std::unordered_map<std::string, size_t> GroupIndices;
        std::unordered_map<std::string, std::map<VkShaderStageFlagBits, std::string>> ShaderGroups;
        std::unordered_map<std::string, std::vector<std::string>> GroupTags;
        std::unordered_map<std::string, std::vector<std::string>> GroupExtensions;
        std::unique_ptr<LuaEnvironment> environment;

        void parseScript();
    };

}

#endif //!ST_SHADER_PACK_SCRIPT_HPP
