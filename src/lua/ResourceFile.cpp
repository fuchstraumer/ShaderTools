#include "lua/ResourceFile.hpp"
#include "parser/ShaderResource.hpp"
#include "luabridge/LuaBridge.h"
#include "generation/ShaderGenerator.hpp"
#include <iostream>
namespace st {

    ResourceFile::ResourceFile(LuaEnvironment* _env, const char* fname) : env(_env) {
        using namespace luabridge;
        getGlobalNamespace(_env->GetState())
            .addFunction("GetWindowX", RetrievalCallbacks.GetScreenSizeX)
            .addFunction("GetWindowY", RetrievalCallbacks.GetScreenSizeY)
            .addFunction("GetZNear", RetrievalCallbacks.GetZNear)
            .addFunction("GetZFar", RetrievalCallbacks.GetZFar)
            .addFunction("GetFieldOfViewY", RetrievalCallbacks.GetFOVY);
        
        if (luaL_dofile(_env->GetState(), fname)) {
            std::string err = lua_tostring(_env->GetState(), -1);
            std::cerr << err;
            throw std::runtime_error(err.c_str());
        }

        parseResources();
    }

    void ResourceFile::parseResources() {
        using namespace luabridge;

        LuaRef set_table = getGlobal(env->GetState(), "Resources");
        auto resource_sets = env->GetTableMap(set_table);

        for (auto& entry : resource_sets) {
            // Now accessing single "group" of resources
            setResources.emplace(entry.first, set_resource_map_t{});

            auto per_set_resources = env->GetTableMap(entry.second);
            for (auto& set_resource : per_set_resources) {

                // Now accessing/parsing single resource per set
                auto set_resource_data = env->GetTableMap(set_resource.second);
                std::string type_of_resource = set_resource_data.at("Type");

                if (type_of_resource == "UniformBuffer") {
                    UniformBuffer buff;
                    auto buffer_resources = env->GetTableMap(set_resource_data.at("Members"));
                    for (auto& rsrc : buffer_resources) {
                        std::string name = rsrc.first;
                        std::string type = rsrc.second;
                        buff.MemberTypes.emplace_back(name, type);
                    }
                    setResources[entry.first].emplace(set_resource.first, std::move(buff));
                }
                else if (type_of_resource == "StorageImage") {
                    std::string format = set_resource_data.at("Format");
                    size_t size = static_cast<size_t>(int(set_resource_data.at("Size")));
                    setResources[entry.first].emplace(set_resource.first, StorageImage{ format, size });
                }
                else if (type_of_resource == "StorageBuffer") {
                    std::string element_type = set_resource_data.at("ElementType");
                    size_t num_elements = static_cast<size_t>(int(set_resource_data.at("NumElements")));
                    setResources[entry.first].emplace(set_resource.first, StorageBuffer{ element_type, num_elements });
                }
            }
        }

    }

}