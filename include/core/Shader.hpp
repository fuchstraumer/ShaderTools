#pragma once
#ifndef VPSK_SHADER_GROUP_HPP
#define VPSK_SHADER_GROUP_HPP
#include "common/CommonInclude.hpp"
#include "common/ShaderStage.hpp"
#include "common/UtilityStructs.hpp"
#include "reflection/ReflectionStructs.hpp"

namespace st
{

    class ShaderGroupImpl;
    class ShaderPackImpl;
    class ShaderReflectorImpl;
    class ResourceUsage;
    struct yamlFile;
    struct Session;
    struct SessionImpl;

   /**
    * @brief Represents a collection of shader stages that are used together in a single pipeline object.
    * 
    * Groups together shader stages that are used together in a single pipeline object, or represents a single compute shader. Contains an instance of @ShaderReflector
    * to provide reflection information about the shader stages, and provides methods to retrieve all the information you should need to construct a pipeline object for this
    * set of shader stages.
    * 
    * @see ShaderStage
    * @see ShaderReflector
    */
    class ST_API Shader
    {
        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;
    public:

        Shader(
            const char* group_name,
            const size_t num_stages,
            const ShaderStage* stages,
            yamlFile* resource_file_path,
            SessionImpl* error_session_impl);
        ~Shader();
        Shader(Shader&& other) noexcept;
        Shader& operator=(Shader&& other) noexcept;

        /** 
         * @brief Returns the input attributes needed for the stage described by given stage flags. Call once to get number of attributes, then call again with the correctly sized output buffer.
         * @param stage The shader stage to get input attributes for
         * @param num_attrs Pointer to a size_t that will be set to the number of attributes in the output buffer
         * @param attributes Pointer to an array of st::VertexAttributeInfo that will be filled with the input attributes for the shader stage
         * @note The output buffer must be large enough to hold the number of attributes returned by `num_attrs`
         */
        void GetInputAttributes(const VkShaderStageFlags stage, size_t* num_attrs, VertexAttributeInfo* attributes) const;
        /** 
         * @brief Returns the output attributes needed for the stage described by given stage flags. Call once to get number of attributes, then call again with the correctly sized output buffer.
         * @param stage The shader stage to get output attributes for
         * @param num_attrs Pointer to a size_t that will be set to the number of attributes in the output buffer
         * @param attributes Pointer to an array of st::VertexAttributeInfo that will be filled with the output attributes for the shader stage
         * @note The output buffer must be large enough to hold the number of attributes returned by `num_attrs`
         */
        void GetOutputAttributes(const VkShaderStageFlags stage, size_t* num_attrs, VertexAttributeInfo* attributes) const;
        /**
         * @brief Returns the push constant information for the given shader stage.
         * @param stage The shader stage to get push constant information for
         * @return PushConstantInfo structure containing the push constant information for the shader stage
         */
        [[nodiscard]] PushConstantInfo GetPushConstantInfo(const VkShaderStageFlags stage) const;
        /**
         * @brief Returns the st::ShaderStage structs contained in this shader.
         * @param num_stages Pointer to a size_t that will be set to the number of stages in the output buffer
         * @param stages Pointer to an array of ShaderStage that will be filled with the shader stages
         * @note The output buffer must be large enough to hold the number of stages returned by `num_stages`
         */
        void GetShaderStages(size_t* num_stages, ShaderStage* stages) const;
        /**
         * @brief Returns the compiled SPIR-V shader binary for the given shader stage. Will be optimized binary if optimization is enabled and compile succeeded.
         * @param handle The shader stage to get the binary for
         * @param binary_size Pointer to a size_t that will be set to the size of the binary
         * @param dest_binary_ptr Pointer to a buffer that will be filled with the binary data
         * @note The output buffer must be large enough to hold the binary data
         */
        ShaderToolsErrorCode GetShaderBinary(const ShaderStage& handle, size_t* binary_size, uint32_t* dest_binary_ptr) const;
        /**
         * @brief Returns the descriptor set layout bindings for the given descriptor set index.
         * @param set_idx The index of the descriptor set to get the bindings for
         * @param num_bindings Pointer to a size_t that will be set to the number of bindings in the output buffer
         * @param bindings Pointer to an array of VkDescriptorSetLayoutBinding that will be filled with the bindings for the descriptor set
         * @note The output buffer must be large enough to hold the number of bindings returned by `num_bindings`
         */
        void GetSetLayoutBindings(const size_t& set_idx, size_t* num_bindings, VkDescriptorSetLayoutBinding* bindings) const;
        /**
         * @brief Returns the specialization constants used in the shader stages contained in this Shader.
         * @param num_constants Pointer to a size_t that will be set to the number of specialization constants in the output buffer
         * @param constants Pointer to an array of SpecializationConstant that will be filled with the specialization constants
         * @note The output buffer must be large enough to hold the number of specialization constants returned by `num_constants`
         */
        void GetSpecializationConstants(size_t* num_constants, SpecializationConstant* constants) const;
        /**
         * @brief Returns information about resource usages in the shader stages contained in this Shader
         * ResourceUsage objects are used to describe how resources contained by the parent st::ShaderPack are used by this set of shader stages.
         * @see st::ResourceUsage for full documentation on what this structure describes
         * @param set_idx The index of the descriptor set to get the resource usages for
         * @param num_resources Pointer to a size_t that will be set to the number of resources in the output buffer
         * @param resources Pointer to an array of ResourceUsage that will be filled with the resource usages for the descriptor set
         * @note The output buffer must be large enough to hold the number of resources returned by `num_resources`
         */
        void GetResourceUsages(const size_t& set_idx, size_t* num_resources, ResourceUsage* resources) const;
        /**
         * @brief Returns an OR'd mask of all the shader stages contained in this Shader
         */
        VkShaderStageFlags Stages() const noexcept;
        /**
         * @brief Returns if optimization was enabled for a given shader stage
         * @param handle The shader stage to check optimization for
         * @return true if optimization was enabled for the shader stage, false otherwise
         */
        bool OptimizationEnabled(const ShaderStage& handle) const noexcept;
        /**
         * @brief Returns the descriptor set index for a given resource group name
         * @param name The name of the resource group to get the index for
         * @return The index of the descriptor set for the resource group, or std::numeric_limits<uint32_t>::max() if the resource group does not exist
         */
        uint32_t ResourceGroupSetIdx(const char* name) const;
        
        /**
         * @brief Returns the tags associated with this shader group, if any were found
         * Tags can be used by frontends to drive further specialized behavior for shaders, such as describing a depth-only pass or that it's used with a certain lighting model/technique.
         * @return A dll_retrieved_strings_t object containing the tags associated with this shader group
         */
        dll_retrieved_strings_t GetTags() const;
        /**
         * @brief Get the names of all the resources attached to a given descriptor set index.
         * @param set_idx The index of the descriptor set to get the resource names for
         * @return A dll_retrieved_strings_t object containing resource names attached to the descriptor set
         */
        dll_retrieved_strings_t GetSetResourceNames(const uint32_t set_idx) const;
        /**
         * @brief Returns the names of all the resource groups used by this shader
         * @return A dll_retrieved_strings_t object containing the names of all the resource groups used by this shader
         */
        dll_retrieved_strings_t GetUsedResourceGroups() const;
        /**
         * @brief Returns the number of descriptor sets required by this shader group.
         * @return size_t of descriptor sets required by this shader group
         */
        size_t GetNumSetsRequired() const;
        /**
         * @brief Returns the index of this shader group in the shader pack it belongs to.
         * @return size_t index of this shader group in the shader pack
         */
        size_t GetIndex() const noexcept;

        /**
         * @brief Sets the index of this shader group in the shader pack it belongs to.
         * @param _idx The index to set for this shader group
         */
        void SetIndex(size_t _idx);
        /**
         * @brief Sets the tags associated with this shader group.
         * @param num_tags The number of tags to set
         * @param tag_strings An array of tag strings to set
         */
        void SetTags(const size_t num_tags, const char** tag_strings);

    protected:
        friend class ShaderPackImpl;
        ShaderReflectorImpl* GetShaderReflectorImpl();
        const ShaderReflectorImpl* GetShaderReflectorImpl() const;
        Shader(const char * group_name, const size_t num_extensions, const char* const* extensions, const size_t num_includes, const char* const* paths);
    private:
        std::unique_ptr<ShaderGroupImpl> impl;
    };

}

#endif //!VPSK_SHADER_GROUP_HPP
