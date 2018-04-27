#pragma once
#ifndef ST_RESOURCE_FILE_HPP
#define ST_RESOURCE_FILE_HPP
#include "common/CommonInclude.hpp"
#include "LuaEnvironment.hpp"
#include "core/ShaderResource.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
#include <map>
namespace st {

    using set_resource_map_t = std::map<std::string, ShaderResource>;

    class ResourceFile {
        ResourceFile(const ResourceFile&) = delete;
        ResourceFile& operator=(const ResourceFile&) = delete;
    public:

        ResourceFile();
        void Execute(const char* fname);
        const bool& IsReady() const noexcept;
        const set_resource_map_t& GetResources(const std::string& block_name) const;
        const std::unordered_map<std::string, set_resource_map_t>& GetAllResources() const noexcept;

    private:

        std::vector<ShaderResourceSubObject> getBufferSubobjects(ShaderResource & parent_resource, const std::unordered_map<std::string, luabridge::LuaRef>& subobject_table) const;
        ShaderResource createUniformBufferResources(const std::string & parent_name, const std::string & name, const std::unordered_map<std::string, luabridge::LuaRef>& table);
        ShaderResource createStorageBufferResource(const std::string & parent_name, const std::string & name, const std::unordered_map<std::string, luabridge::LuaRef>& table);
        ShaderResource createStorageImageResource(const std::string & parent_name, const std::string & name, const std::unordered_map<std::string, luabridge::LuaRef>& table);

        bool ready{ false };
        std::unordered_map<std::string, set_resource_map_t> setResources;
        std::unique_ptr<LuaEnvironment> environment;
        void parseResources();
    };

}

#endif //!ST_RESOURCE_FILE_HPP