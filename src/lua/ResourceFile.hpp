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

    class ResourceFile {
        ResourceFile(const ResourceFile&) = delete;
        ResourceFile& operator=(const ResourceFile&) = delete;
    public:

        ResourceFile();
        void Execute(const char* fname);
        const std::vector<ShaderResource>& GetResources(const std::string& block_name) const;
        const std::unordered_map<std::string, std::vector<ShaderResource>>& GetAllResources() const noexcept;
        const ShaderResource* FindResource(const std::string& name) const;
        void AddResourceGroup(const std::string& block_name, const std::vector<ShaderResource>& resources);
        
    private:

        const ShaderResource* searchSingleGroupForResource(const std::string& group, const std::string& name) const;

        VkImageCreateInfo parseImageOptions(ShaderResource& rsrc, const std::unordered_map<std::string, luabridge::LuaRef>& image_info_table) const;
        VkImageViewCreateInfo parseImageViewOptions(const std::unordered_map<std::string, luabridge::LuaRef>& view_info_table, LuaEnvironment* env, const VkImageCreateInfo& parent_image_info) const;
        VkSamplerCreateInfo parseSamplerOptions(const std::unordered_map<std::string, luabridge::LuaRef>& sampler_info_table) const;
        VkBufferViewCreateInfo getStorageImageBufferViewInfo(ShaderResource & rsrc) const;
        ShaderResourceSubObject createSimpleBufferSubresource(const std::string & name, const luabridge::LuaRef & object_ref, uint32_t& offset_total) const;
        ShaderResourceSubObject createComplexBufferSubresource(const std::string & name, const luabridge::LuaRef & object_ref, uint32_t & offset_total) const;
        std::vector<ShaderResourceSubObject> getBufferSubobjects(ShaderResource & parent_resource, const std::unordered_map<std::string, luabridge::LuaRef>& subobject_table) const;

        void setBaseResourceInfo(const std::string & parent_name, const std::string & name, const VkDescriptorType type, ShaderResource & rsrc) const;
        void createUniformBufferResources(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;
        void createStorageBufferResource(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;
        void createTexelBufferResource(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;
        void createCombinedImageSamplerResource(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;
        void createSampledImageResource(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;
        void createSamplerResource(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource& rsrc) const;

        void createStorageImageResource(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource & rsrc) const;

        std::unordered_map<std::string, std::vector<ShaderResource>> setResources;
        std::unique_ptr<LuaEnvironment> environment;
        void createInputAttachmentResource(const std::unordered_map<std::string, luabridge::LuaRef>& table, ShaderResource & rsrc) const;
        void parseResources();
    };

}

#endif //!ST_RESOURCE_FILE_HPP
