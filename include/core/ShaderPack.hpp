#pragma once
#ifndef ST_SHADER_PACK_HPP
#define ST_SHADER_PACK_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"
#include "common/stSession.hpp"

namespace st
{

    class ShaderPackImpl;
    class Shader;
    class ShaderResource;
    class ResourceGroup;

    /**
     * @brief Represents a collection of shader groups (describing shader stages in a pipeline) and resource groups (describing resources used by those shaders), constructed from a YAML file.
     * 
     * This class is the main entry point for working with shaders in this library. Pass it a YAML file to begin the parsing process, and it will create shader groups for each group defined
     * YAML file. It will then go through and compile the shaders, generating reflection info about what resources were used in each shader group, and how the resources were accessed within
     * said shader group (when applicable, not possible with bindless). Once this construction is complete, you will then be using st::Shader as your main interface to get further information
     * about each shader and prepare pipelines for construction in your application.'
     * 
     * @note First requires a valid st::Session object to be created, as this is what we store all our errors and warnings in.
     * @ingroup Core
     */
    class ST_API ShaderPack
    {
        ShaderPack(const ShaderPack&) = delete;
        ShaderPack& operator=(const ShaderPack&) = delete;
    public:

        ShaderPack(const char* shader_pack_yaml_path, Session& session);
        ~ShaderPack();

        /**
         * @brief Returns list of names of st::Shaders contained in this ShaderPack.
         * @return dll_retrieved_strings_t A collection of strings containing the names of all shader groups in this ShaderPack
         */
        dll_retrieved_strings_t GetShaderGroupNames() const;

        /**
         * @brief Returns the st::Shader object for a given group name. Intended to be used after a call to GetShaderGroupNames() to obtain the list of all shader groups in the pack.
         * @param name The name of the shader group to retrieve
         * @return const Shader* Pointer to the Shader object for the specified group, or nullptr if not found
         */
        const Shader* GetShaderGroup(const char* name) const;

        /**
         * @brief Returns the names of all resource groups used by this ShaderPack.
         * @return dll_retrieved_strings_t A collection of strings containing the names of all resource groups in this ShaderPack
         */
        dll_retrieved_strings_t GetResourceGroupNames() const;

        /**
         * @brief Returns the resource group for a given resource group name. Intended to be used after a call to GetResourceGroupNames() to obtain the list of all resource groups in the pack.
         * @param name The name of the resource group to retrieve
         * @return const ResourceGroup* Pointer to the ResourceGroup object for the specified group, or nullptr if not found
         */
        const ResourceGroup* GetResourceGroup(const char* name) const;

        /**
         * @brief descriptor_type_counts_t Returns the total counts of descriptor types used by all shader groups in this ShaderPack.
         * @return descriptor_type_counts_t A structure containing the total counts of each descriptor type used in this whole ShaderPack
         * @note You can use this to set up things like descriptor pools, or to determine how many descriptors you need to allocate for the whole pack.
         */
        const descriptor_type_counts_t& GetTotalDescriptorTypeCounts() const;

        /**
         * @brief Returns the st::ShaderResource object for a given resource name.
         * @param rsrc_name The name of the resource to retrieve
         * @note This will search through all the resource groups in this ShaderPack, so it can be slow. Avoid using this if you can.
         */
        const ShaderResource* GetResource(const char* rsrc_name) const;
        
    private:
        friend struct ShaderFileTracker;
        friend struct ShaderPackBinary;
        std::unique_ptr<ShaderPackImpl> impl;
    };

}

#endif //ST_SHADER_PACK_HPP
