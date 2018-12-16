#include "ShaderPackScript.hpp"
#include "easyloggingpp/src/easylogging++.h"
#ifdef SHADERTOOLS_PROFILING_ENABLED
#include <chrono>
#endif

namespace st {

    ShaderPackScript::ShaderPackScript(const char * fname) : environment(std::make_unique<LuaEnvironment>()) {
#ifdef SHADERTOOLS_PROFILING_ENABLED
        std::chrono::high_resolution_clock::time_point beforeExec;
        beforeExec = std::chrono::high_resolution_clock::now();
#endif // SHADERTOOLS_PROFILING_ENABLED
        environment->Execute(fname);
        parseScript();
#ifdef SHADERTOOLS_PROFILING_ENABLED
        std::chrono::duration<double, std::milli> work_time = std::chrono::high_resolution_clock::now() - beforeExec;
        LOG(INFO) << "Execution and parsing of ShaderPack Lua script took " << work_time.count() << "ms";
#endif // SHADERTOOLS_PROFILING_ENABLED
    }

    void ShaderPackScript::parseScript() {
        using namespace luabridge;
        lua_State* state = environment->GetState();
        {
            LuaRef pack_name_ref = getGlobal(state, "PackName");
            PackName = pack_name_ref.cast<std::string>();
            LuaRef resource_file_ref = getGlobal(state, "ResourceFileName");
            ResourceFileName = resource_file_ref.cast<std::string>();

            LuaRef shader_groups_table = getGlobal(state, "ShaderGroups");
            auto shader_groups = environment->GetTableMap(shader_groups_table);
            for (const auto& group : shader_groups) {
                auto group_table = environment->GetTableMap(group.second);
                size_t idx = static_cast<size_t>(group_table.at("Idx").cast<int>());
                GroupIndices.emplace(group.first, std::move(idx));
                auto group_entries = environment->GetTableMap(group_table.at("Shaders"));
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

                if (group_table.count("Tags") != 0) {
                    auto tags_table = environment->GetLinearTable(group_table.at("Tags"));
                    std::vector<std::string> tag_strings;
                    for (auto& ref : tags_table) {
                        tag_strings.emplace_back(ref.cast<std::string>());
                    }
                    GroupTags.emplace(group.first, tag_strings);
                }

                if (group_table.count("Extensions") != 0) {
                    auto extensions_table = environment->GetLinearTable(group_table.at("Extensions"));
                    std::vector<std::string> extension_strings;
                    for (auto& ref : extensions_table) {
                        extension_strings.emplace_back(ref.cast<std::string>());
                    }
                    GroupExtensions.emplace(group.first, extension_strings);
                }

            }
        }
        environment.reset();
    }

}
