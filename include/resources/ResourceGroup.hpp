#pragma once
#ifndef SHADERTOOLS_RESOURCE_GROUP_HPP
#define SHADERTOOLS_RESOURCE_GROUP_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"

namespace st
{

    class ShaderResource;
    class ResourceGroupImpl;
    struct yamlFile;
    struct ShaderStage;
    struct Session;
    struct SessionImpl;

    /**
     * @brief Represents a collection of resources used by shaders with the USE_RESOURCE macro, organized by the YAML file they were defined in.
     * There is no requirement on how resource groups are organized or defined, you are free to organize them however you like. End-users should
     * stick to the const functions that don't mutate the state of this object.
     * @note A resource group usually ends up representing a singular descriptor set in the final pipeline, but this is not a requirement.
     * @see ShaderResource for the struct giving more info about individual resources
     * @ingroup Resources
     */
    class ST_API ResourceGroup
    {
        ResourceGroup(const ResourceGroup&) = delete;
        ResourceGroup& operator=(const ResourceGroup&) = delete;
        friend class ResourceFile;
    public:

        ResourceGroup(yamlFile* resource_file, const char* group_name, SessionImpl* error_session);
        ~ResourceGroup();

        /**
         * @brief Retrieves names of all resources in this group
         * @return dll_retrieved_strings_t A collection of strings containing the names of all resources in this group
         */
        dll_retrieved_strings_t GetResourceNames() const noexcept;
        /**
         * @brief Retrieves names of all st::Shader objects that use this resource group
         * @return dll_retrieved_strings_t A collection of strings containing the names of all st::Shader objects that use this resource group
         */
        dll_retrieved_strings_t GetNamesOfShadersThatUseThisGroup() const noexcept;
        /**
         * @brief Returns any user-defined tags associated with this resource group, if any were found during YAML file parsing.
         * @return dll_retrieved_strings_t A collection of strings containing the user-defined tags associated with this resource group
         */
        dll_retrieved_strings_t GetTags() const noexcept;
        /**
         * @brief Returns the counts of descriptor types used in this particualr resource group
         * @return descriptor_type_counts_t A structure containing the counts of each descriptor type used in this resource group
         * @note This is much less useful in bindless mode, when most of these counts will be zero or one since everything will be a single descriptor array
         */
        const descriptor_type_counts_t& DescriptorCounts() const noexcept;

        const char* Name() const noexcept;

        /**
         * @brief Returns the index of the descriptor set for this resource group in the given shader stage it is used in
         * @param handle The shader stage to get the descriptor set index for
         * @return uint32_t The index of the descriptor set for this resource group in the given shader stage, or std::numeric_limits<uint32_t>::max() if the resource group is not used in that shader stage
         */
        uint32_t DescriptorSetIdxInStage(const ShaderStage& handle) const;

        size_t GetNumResources() const noexcept;
        ShaderResource* operator[](const char* name) noexcept;
        const ShaderResource* operator[](const char* name) const noexcept;

        /**
         * @brief Deep-copies the ShaderResource struct to the given destination pointer. Call twice, first to get the number of resources to size your destination buffer correctly. Call again to write to the buffer.
         * @param num_resources A pointer to a size_t variable that will be filled with the number of resources in this group
         * @param resources A pointer to a buffer to write the resources to, sized based on the first call and num_resources to ensure it is large enough.
         * @note The output buffer must be large enough to hold the number of resources returned by the first call, or behavior is undefined (read: you'll segfault).
         */
        void GetResources(size_t* num_resources, ShaderResource* resources) const;
        /**
         * @brief Copies only the pointers to the st::ShaderResource structs attached to this resource group. Call twice, first to get the number of resources to size your destination buffer correctly. Call again to write to the buffer.
         * @param num_resources A pointer to a size_t variable that will be filled with the number of resources in this group
         * @param resources A pointer to a buffer to write the resource pointers to, sized based on the first call and num_resources to ensure it is large enough.
         * @note The output buffer must be large enough to hold the number of resource pointers returned by the first call, or behavior is undefined (read: you'll segfault).
         */
        void GetResourcePtrs(size_t* num_resources, const ShaderResource** resources) const;

        void SetName(const char* _name);
        void UsedByGroup(const char* new_group);
        void SetTags(const size_t num_tags, const char** tags);

    private:
        std::unique_ptr<ResourceGroupImpl> impl;
    };

}

#endif //!SHADERTOOLS_RESOURCE_GROUP_HPP
