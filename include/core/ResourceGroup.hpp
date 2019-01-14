#pragma once
#ifndef SHADERTOOLS_RESOURCE_GROUP_HPP
#define SHADERTOOLS_RESOURCE_GROUP_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"

namespace st {

    class ShaderResource;
    class ResourceGroupImpl;
    struct yamlFile;
    struct ShaderStage;

    class ST_API ResourceGroup {
        ResourceGroup(const ResourceGroup&) = delete;
        ResourceGroup& operator=(const ResourceGroup&) = delete;
        friend class ResourceFile;
    public:

        ResourceGroup(yamlFile* resource_file, const char* group_name);
        ~ResourceGroup();

        dll_retrieved_strings_t ResourceNames() const noexcept;
        dll_retrieved_strings_t UsedByGroups() const noexcept;
        dll_retrieved_strings_t GetTags() const noexcept;
        const descriptor_type_counts_t& DescriptorCounts() const noexcept;
        const char* Name() const noexcept;
        uint32_t DescriptorSetIdxInStage(const ShaderStage& handle) const;
        ShaderResource* operator[](const char* name) noexcept;
        const ShaderResource* operator[](const char* name) const noexcept;
        void GetResources(size_t* num_resources, ShaderResource* resources) const;
        void GetResourcePtrs(size_t* num_resources, const ShaderResource** resources) const;

        void SetName(const char* _name);
        void UsedByGroup(const char* new_group);
        void SetTags(const size_t num_tags, const char** tags);

    private:
        std::unique_ptr<ResourceGroupImpl> impl;
    };

}

#endif //!SHADERTOOLS_RESOURCE_GROUP_HPP
