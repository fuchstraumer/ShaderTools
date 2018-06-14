#pragma once
#ifndef SHADERTOOLS_RESOURCE_GROUP_HPP
#define SHADERTOOLS_RESOURCE_GROUP_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"
namespace st {

    class ShaderResource;
    class ShaderPack;
    class ShaderGroup;
    class ResourceGroupImpl;
    class ResourceFile;

    class ST_API ResourceGroup {
        ResourceGroup(const ResourceGroup&) = delete;
        ResourceGroup& operator=(const ResourceGroup&) = delete;
        friend class ResourceFile;
    public:

        ResourceGroup(ResourceFile* resource_file, const char* group_name);
        ~ResourceGroup();

        dll_retrieved_strings_t ResourceNames() const noexcept;
        dll_retrieved_strings_t UsedByGroups() const noexcept;
        ShaderResource* operator[](const char* name);
        const ShaderResource* operator[](const char* name) const;
        const char* Name() const noexcept;
        dll_retrieved_strings_t GetTags() const noexcept;

        void SetName(const char* _name);
        void UsedByGroup(const char* new_group);
        void SetTags(const size_t num_tags, const char** tags);

    private:
        std::unique_ptr<ResourceGroupImpl> impl;
    };

}

#endif //!SHADERTOOLS_RESOURCE_GROUP_HPP
