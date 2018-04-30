#pragma once
#ifndef ST_RESOURCE_FILE_HPP
#define ST_RESOURCE_FILE_HPP
#include "common/CommonInclude.hpp"
#include "LuaEnvironment.hpp"
#include "core/ShaderResource.hpp"
#include "core/ResourceUsage.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
namespace st {

    using set_resource_map_t = std::vector<ShaderResource>;

    class ResourceFile {
        ResourceFile(const ResourceFile&) = delete;
        ResourceFile& operator=(const ResourceFile&) = delete;
    public:

        ResourceFile();
        void Execute(const char* fname);
        const set_resource_map_t& GetResources(const std::string& block_name) const;
        const std::unordered_map<std::string, set_resource_map_t>& GetAllResources() const noexcept;
        const ShaderResource* FindResource(const std::string& name) const;

    private:

        const ShaderResource* searchSingleGroupForResource(const std::string& group, const std::string& name) const;

        VkImageCreateInfo parseImageOptions(ShaderResource& rsrc, const std::unordered_map<std::string, luabridge::LuaRef>& image_info_table) const;
        VkSamplerCreateInfo parseSamplerOptions(ShaderResource& rsrc, const std::unordered_map<std::string, luabridge::LuaRef>& sampler_info_table) const;
        VkBufferViewCreateInfo getStorageImageBufferViewInfo(ShaderResource & rsrc) const;
        ShaderResourceSubObject createSimpleBufferSubresource(const std::string & name, const luabridge::LuaRef & object_ref, uint32_t& offset_total) const;
        ShaderResourceSubObject createComplexBufferSubresource(const std::string & name, const luabridge::LuaRef & object_ref, uint32_t & offset_total) const;
        std::vector<ShaderResourceSubObject> getBufferSubobjects(ShaderResource & parent_resource, const std::unordered_map<std::string, luabridge::LuaRef>& subobject_table) const;

        void setBaseResourceInfo(const std::string & parent_name, const std::string & name, const VkDescriptorType type, ShaderResource & rsrc) const;
        void createUniformBufferResources(const std::string & parent_name, const std::string & name, const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;
        void createStorageBufferResource(const std::string & parent_name, const std::string & name, const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;
        void createStorageTexelBufferResource(const std::string & parent_name, const std::string & name, const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;
        void createCombinedImageSamplerResource(const std::string & parent_name, const std::string & name, const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;
        void createSampledImageResource(const std::string & parent_name, const std::string & name, const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;
        void createSamplerResource(const std::string & parent_name, const std::string & name, const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;

        std::unordered_map<std::string, set_resource_map_t> setResources;
        std::unique_ptr<LuaEnvironment> environment;
        void parseResources();
    };

}

#endif //!ST_RESOURCE_FILE_HPP