#include "ResourceFile.hpp"
#include "generation/ShaderGenerator.hpp"
#include "core/ShaderGroup.hpp"
#include "common/UtilityStructs.hpp"
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

    ShaderResource ResourceFile::createUniformBufferResources(const std::string& parent_name, const std::string& name, const std::unordered_map<std::string, luabridge::LuaRef>& table) {
        auto& f_tracker = ShaderFileTracker::GetFileTracker();

        ShaderResource s_resource;
        s_resource.SetParentGroupName(parent_name.c_str());
        s_resource.SetName(name.c_str());
        s_resource.SetType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        std::vector<ShaderResourceSubObject> members;
        auto buffer_resources = environment->GetTableMap(table.at("Members"));

        uint32_t offset_total = 0;
        for (auto& rsrc : buffer_resources) {
            ShaderResourceSubObject object;
            object.Name = rsrc.first;
            auto iter = f_tracker.ObjectSizes.find(rsrc.second);
            if (iter != f_tracker.ObjectSizes.cend()) {
                object.Size = static_cast<uint32_t>(iter->second);
                object.Offset = offset_total;
                offset_total += object.Size;
            }
            else {
                throw std::runtime_error("Couldn't find resources size.");
            }
            members.emplace_back(object);
        }

        s_resource.SetMemoryRequired(static_cast<size_t>(offset_total));
        s_resource.SetMembers(members.size(), members.data());

        return s_resource;
    }

    ShaderResource ResourceFile::createStorageBufferResource(const std::string& parent_name, const std::string& name, const std::unordered_map<std::string, luabridge::LuaRef>& table) {
        auto& f_tracker = ShaderFileTracker::GetFileTracker();
        ShaderResource s_resource;
        s_resource.SetParentGroupName(parent_name.c_str());
        s_resource.SetName(name.c_str());
        s_resource.SetType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        std::string element_type = table.at("ElementType").cast<std::string>();
        auto iter = f_tracker.ObjectSizes.find(element_type);
        if (iter != f_tracker.ObjectSizes.cend()) {
            const size_t& element_size = iter->second;
            size_t num_elements = static_cast<size_t>(table.at("NumElements").cast<int>());
            s_resource.SetMemoryRequired(element_size * num_elements);
        }
        else {
            throw std::runtime_error("Couldn't find resources size.");
        }
        return s_resource;
    }

    ShaderResource ResourceFile::createStorageImageResource(const std::string& parent_name, const std::string& name, const std::unordered_map<std::string, luabridge::LuaRef>& table) {
        auto& f_tracker = ShaderFileTracker::GetFileTracker();
        ShaderResource s_resource;
        s_resource.SetParentGroupName(parent_name.c_str());
        s_resource.SetName(name.c_str());
        s_resource.SetType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        std::string format_str = table.at("Format").cast<std::string>();
        s_resource.SetFormat(StorageImageFormatToVkFormat(format_str.c_str()));
        size_t image_size = static_cast<size_t>(table.at("Size").cast<int>());
        size_t footprint = MemoryFootprintForFormat(s_resource.GetFormat());
        if (footprint != std::numeric_limits<size_t>::max()) {
            s_resource.SetMemoryRequired(footprint * image_size);
        }
        return s_resource;
    }

    void ResourceFile::parseResources() {
        using namespace luabridge;

        auto& f_tracker = ShaderFileTracker::GetFileTracker();

        LuaRef size_ref = getGlobal(environment->GetState(), "ObjectSizes");
        if (!size_ref.isNil()) {
            auto size_table = environment->GetTableMap(size_ref);
            for (auto & entry : size_table) {
                size_t size = static_cast<size_t>(entry.second.cast<int>());
                f_tracker.ObjectSizes.emplace(entry.first, std::move(size));
            }
        }

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

                if (type_of_resource == "UniformBuffer") {
                    setResources[entry.first].emplace(createUniformBufferResources(entry.first, set_resource.first, set_resource_data));
                }
                else if (type_of_resource == "StorageImage") {
                    setResources[entry.first].emplace(createStorageImageResource(entry.first, set_resource.first, set_resource_data));
                }
                else if (type_of_resource == "StorageBuffer") {
                    setResources[entry.first].emplace(createStorageBufferResource(entry.first, set_resource.first, set_resource_data));
                }
            }
        }
    }

}