#pragma once
#ifndef ST_SHADER_RESOURCE_HPP
#define ST_SHADER_RESOURCE_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"
#include "common/ShaderStage.hpp"

namespace st
{

    class ShaderResourceImpl;
    struct ShaderResourceSubObject;

    enum class glsl_qualifier : uint16_t
    {
        Coherent = 0,
        ReadOnly,
        WriteOnly,
        Volatile,
        Restrict,
        InvalidQualifier = std::numeric_limits<uint16_t>::max()
    };

    /**
     * @brief Describes a single resource used by shaders, including multiple shaders. Provides core type information, binding indices when applicable, and more. 
     * Frontends can use this to understand how resources are used across multiple shaders, how to construct descriptor sets, and how to optimize or structure pipelines
     * and pipeline dispatches to ensure data validity and correctness.
     * @ingroup Resources
     */
    class ST_API ShaderResource
    {
    public:

        ShaderResource();
        ~ShaderResource();
        ShaderResource(const ShaderResource& other) noexcept;
        ShaderResource(ShaderResource&& other) noexcept;
        ShaderResource& operator=(const ShaderResource& other) noexcept;
        ShaderResource& operator=(ShaderResource&& other) noexcept;

        /** @note This function returns std::numeric_limits<size_t>::max() if the resource is an input attachment. */
        size_t BindingIndex() const noexcept;
        /** @note This function returns std::numeric_limits<size_t>::max() if the resource is not an input attachment. */
        size_t InputAttachmentIndex() const noexcept;
        /** @note This function returns VK_FORMAT_UNDEFINED if the underlying resource does not have a format (i.e., every non-image resource type) */
        VkFormat Format() const noexcept;

        const char* Name() const;
        const char* ParentGroupName() const;
        VkShaderStageFlags ShaderStages() const noexcept;
        VkDescriptorType DescriptorType() const noexcept;
        operator VkDescriptorSetLayoutBinding() const noexcept;
        VkDescriptorSetLayoutBinding AsLayoutBinding() const noexcept;
        const char* ImageSamplerSubtype() const;
        bool HasQualifiers() const noexcept;
        /**
         * @brief Returns the qualifiers that are *always* applied to this resource across all invocations/usages of it. 
         * Call twice, first to size the output buffer with `num_qualifiers`, then again with the output buffer sized to that value.
         * @param num_qualifiers Pointer to a size_t that will be set to the number of qualifiers in the output buffer
         * @param qualifiers Pointer to an array of glsl_qualifier to be filled with the qualifiers
         * @note Behavior is undefined if the output buffer is non-null and not sized correctly.
         */
        void GetQualifiers(size_t* num_qualifiers, glsl_qualifier* qualifiers) const noexcept;
        /**
         * @brief Returns to qualifiers applicable to the resource in the given shader stage. 
         * Call twice, first to size the output buffer with `num_qualifiers`, then again with the output buffer sized to that value.
         * @param stage The shader stage to get qualifiers for
         * @param num_qualifiers Pointer to a size_t that will be set to the number of qualifiers in the output buffer
         * @param qualifiers Pointer to an array of glsl_qualifier to be filled with the qualifiers
         * @note Behavior is undefined if the output buffer is non-null and not sized correctly.
         */
        void GetPerUsageQualifiers(ShaderStage stage, size_t* num_qualifiers, glsl_qualifier* qualifiers) const noexcept;
        /** @note Returns glsl_qualifier::Invalid if the resource is not in an exclusive read/write context */
        glsl_qualifier GetReadWriteQualifierForShader(ShaderStage stage) const noexcept;
        dll_retrieved_strings_t GetTags() const noexcept;
        /**
         * @brief If the resource is a structure type, this will return a string representation of the members of the structure as found in the source code.
         * @note This is not guaranteed to be a valid C++ structure definition, but it can be useful for debugging or frontend tools to understand or present the structure of the resource.
         */
        const char* GetMembersStr() const noexcept;
        /** @brief Checks if the resource is used as a descriptor array */
        bool IsDescriptorArray() const noexcept;
        /** 
         * @brief Returns the size of the descriptor array, if it has a bounded size
         * @note This function returns 0 if the resource is an unbounded descrptor array, and std::Numeric_limits<uint32_t>::max() if the resource is not a descriptor array
         */
        uint32_t ArraySize() const noexcept;
        /**
         * @brief Returns where in the parent descriptor set (effectively defined by the parent ResourceGroup) this resource is bound.
         * @note Duplicate indices for multiple resources are possible if in bindless mode, as all resources are bound to the same descriptor set and binding index.
         * @return uint32_t The binding index of this resource in the parent descriptor set
         */
        uint32_t BindingIdx() const noexcept;

        void SetBindingIndex(uint32_t idx);
        void SetInputAttachmentIndex(size_t idx);
        void SetStages(VkShaderStageFlags stages);
        void SetType(VkDescriptorType _type);
        void SetName(const char* name);
        void SetMembersStr(const char* members_str);
        void SetParentGroupName(const char* parent_group_name);
        void SetQualifiers(const size_t num_qualifiers, glsl_qualifier* qualifiers);
        void AddPerUsageQualifier(ShaderStage stage, glsl_qualifier qualifier);
        void AddPerUsageQualifiers(ShaderStage stage, const size_t num_qualifiers, const glsl_qualifier* qualifiers);
        void SetFormat(VkFormat fmt);
        void SetTags(const size_t num_tags, const char** tags);
        void SetImageSamplerSubtype(const char* subtype);
        void SetDescriptorArray(bool val);
        void SetArraySize(uint32_t val);

    private:
        friend struct yamlFile;
        std::unique_ptr<ShaderResourceImpl> impl;
    };

}

#endif //!ST_SHADER_RESOURCE_HPP
