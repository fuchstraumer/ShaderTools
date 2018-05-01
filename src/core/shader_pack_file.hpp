#pragma once
#ifndef ST_SHADER_PACK_FILE_HPP
#define ST_SHADER_PACK_FILE_HPP
#include "common/CommonInclude.hpp"
#include "../lua/LuaEnvironment.hpp"
#include <map>
#include <string>
#include <unordered_map>
namespace st {

    
    struct shader_pack_file_t {

        shader_pack_file_t(const char* fname);
        ~shader_pack_file_t() {};

        std::string PackName;
        std::string ResourceFileName;
        std::unordered_map<std::string, std::map<VkShaderStageFlagBits, std::string>> ShaderGroups;
        std::unique_ptr<LuaEnvironment> environment;

        void parseScript();
    };

    shader_pack_file_t::shader_pack_file_t(const char * fname) : environment(std::make_unique<LuaEnvironment>()) {
        environment->Execute(fname);
        parseScript();
    }

    void shader_pack_file_t::parseScript() {
        using namespace luabridge;
        lua_State* state = environment->GetState();
        {
            LuaRef pack_name_ref = getGlobal(state, "PackName");
            PackName = pack_name_ref.cast<std::string>();
            LuaRef resource_file_ref = getGlobal(state, "ResourceFileName");
            ResourceFileName = resource_file_ref.cast<std::string>();

            LuaRef shader_groups_table = getGlobal(state, "ShaderGroups");
            auto shader_groups = environment->GetTableMap(shader_groups_table);

            for (auto& group : shader_groups) {
                auto group_entries = environment->GetTableMap(group.second);
                for (auto& entry : group_entries) {
                    const std::string& shader_stage = entry.first;
                    const std::string shader_name = entry.second;
                    if (shader_stage == "Vertex") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_VERTEX_BIT, shader_name);
                    }
                    else if (shader_stage == "Fragment") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_FRAGMENT_BIT, shader_name);
                    }
                    else if (shader_stage == "Geometry") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_GEOMETRY_BIT, shader_name);
                    }
                    else if (shader_stage == "TessEval") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, shader_name);
                    }
                    else if (shader_stage == "TessControl") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, shader_name);
                    }
                    else if (shader_stage == "Compute") {
                        ShaderGroups[group.first].emplace(VK_SHADER_STAGE_COMPUTE_BIT, shader_name);
                    }
                    else {
                        throw std::domain_error("Invalid shader stage in parsed Lua script.");
                    }
                }
            }
        }
        environment.reset();
    }

}

#endif //!ST_SHADER_PACK_FILE_HPP