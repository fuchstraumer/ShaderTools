#include "ResourceFile.hpp"
#include "generation/ShaderGenerator.hpp"
#include "core/ShaderGroup.hpp"
#include "../util/ShaderFileTracker.hpp"
#include <iostream>
namespace st {

    ResourceFile::ResourceFile() : environment(std::make_unique<LuaEnvironment>()) {
        using namespace luabridge;
        getGlobalNamespace(environment->GetState())
            .addFunction("GetWindowX", ShaderGroup::RetrievalCallbacks.GetScreenSizeX)
            .addFunction("GetWindowY", ShaderGroup::RetrievalCallbacks.GetScreenSizeY)
            .addFunction("GetZNear", ShaderGroup::RetrievalCallbacks.GetZNear)
            .addFunction("GetZFar", ShaderGroup::RetrievalCallbacks.GetZFar)
            .addFunction("GetFieldOfViewY", ShaderGroup::RetrievalCallbacks.GetFOVY);
    }

    const set_resource_map_t& ResourceFile::GetResources(const std::string & block_name) const {
        return setResources.at(block_name);
    }

    const std::unordered_map<std::string, set_resource_map_t>& ResourceFile::GetAllResources() const noexcept {
        return setResources;
    }

    void ResourceFile::Execute(const char* fname)  {

        if (luaL_dofile(environment->GetState(), fname)) {
            std::string err = lua_tostring(environment->GetState(), -1);
            throw std::logic_error(err.c_str());
        }

        parseResources();
        ready = true;

    }

    const bool & ResourceFile::IsReady() const noexcept {
        return ready;
    }

    void ResourceFile::parseResources() {
        using namespace luabridge;

        LuaRef set_table = getGlobal(environment->GetState(), "Resources");
        auto resource_sets = environment->GetTableMap(set_table);

        for (auto& entry : resource_sets) {
            // Now accessing single "group" of resources
            setResources.emplace(entry.first, set_resource_map_t{});

            auto per_set_resources = environment->GetTableMap(entry.second);
            for (auto& set_resource : per_set_resources) {

                // Now accessing/parsing single resource per set
                auto set_resource_data = environment->GetTableMap(set_resource.second);
                std::string type_of_resource = set_resource_data.at("Type");
                ShaderResource s_resource;
                s_resource.SetParentGroupName(entry.first.c_str());
                s_resource.SetName(set_resource.first.c_str());

                if (type_of_resource == "UniformBuffer") {
                    s_resource.SetType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

                    std::vector<ShaderResourceSubObject> members;
                    auto buffer_resources = environment->GetTableMap(set_resource_data.at("Members"));
                    for (auto& rsrc : buffer_resources) {
                        std::string name = rsrc.first;
                        std::string type = rsrc.second;
                        buff.MemberTypes.emplace_back(name, type);
                    }
                    setResources[entry.first].emplace(set_resource.first, std::move(buff));
                }
                else if (type_of_resource == "StorageImage") {
                    s_resource.SetType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
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