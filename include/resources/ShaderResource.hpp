#pragma once
#ifndef ST_SHADER_RESOURCE_HPP
#define ST_SHADER_RESOURCE_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"

namespace st {

    class ShaderResourceImpl;
    struct ShaderResourceSubObject;

    enum class glsl_qualifier : uint16_t {
        Coherent = 0,
        ReadOnly,
        WriteOnly,
        Volatile,
        Restrict,
        InvalidQualifier = std::numeric_limits<uint16_t>::max()
    };

    class ST_API ShaderResource {
    public:

        ShaderResource();
        ~ShaderResource();
        ShaderResource(const ShaderResource& other) noexcept;
        ShaderResource(ShaderResource&& other) noexcept;
        ShaderResource& operator=(const ShaderResource& other) noexcept;
        ShaderResource& operator=(ShaderResource&& other) noexcept;
        
        size_t BindingIndex() const noexcept;
        size_t InputAttachmentIndex() const noexcept;
        VkFormat Format() const noexcept;

        const char* Name() const;
        const char* ParentGroupName() const;
        VkShaderStageFlags ShaderStages() const noexcept;
        VkDescriptorType DescriptorType() const noexcept;
        operator VkDescriptorSetLayoutBinding() const noexcept;
        VkDescriptorSetLayoutBinding AsLayoutBinding() const noexcept;
        const char* ImageSamplerSubtype() const;
        bool HasQualifiers() const noexcept;
        void GetQualifiers(size_t* num_qualifiers, glsl_qualifier* qualifiers) const noexcept;
        void GetPerUsageQualifiers(const char* shader_name, size_t* num_qualifiers, glsl_qualifier* qualifiers) const noexcept;
        // Returns readonly/writeonly, if available. If not available or not applied to resource, returns glsl_qualifier::Invalid
        glsl_qualifier GetReadWriteQualifierForShader(const char* shader_name) const noexcept;
        dll_retrieved_strings_t GetTags() const noexcept;
        const char* GetMembersStr() const noexcept;
        bool IsDescriptorArray() const noexcept;
        uint32_t ArraySize() const noexcept;
        uint32_t BindingIdx() const noexcept;

        void SetBindingIndex(uint32_t idx);
        void SetInputAttachmentIndex(size_t idx);
        void SetStages(VkShaderStageFlags stages);
        void SetType(VkDescriptorType _type);
        void SetName(const char* name);
        void SetMembersStr(const char* members_str);
        void SetParentGroupName(const char* parent_group_name);
        void SetQualifiers(const size_t num_qualifiers, glsl_qualifier* qualifiers);
        void AddPerUsageQualifier(const char* shader_name, glsl_qualifier qualifier);
        void AddPerUsageQualifiers(const char* shader_name, const size_t num_qualifiers, const glsl_qualifier* qualifiers);
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
