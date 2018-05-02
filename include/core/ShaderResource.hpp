#pragma once
#ifndef ST_SHADER_RESOURCE_HPP
#define ST_SHADER_RESOURCE_HPP
#include "common/CommonInclude.hpp"

namespace st {

    class ShaderResourceImpl;
    struct ShaderResourceSubObject;

    enum class size_class : uint32_t {
        Absolute = 0,
        SwapchainRelative,
        ViewportRelative,
        Invalid = std::numeric_limits<uint32_t>::max()
    };

    class ST_API ShaderResource {
    public:

        ShaderResource();
        ~ShaderResource();
        ShaderResource(const ShaderResource& other) noexcept;
        ShaderResource(ShaderResource&& other) noexcept;
        ShaderResource& operator=(const ShaderResource& other) noexcept;
        ShaderResource& operator=(ShaderResource&& other) noexcept;
        
        const size_t& GetAmountOfMemoryRequired() const noexcept;
        const VkFormat& GetFormat() const noexcept;
        const char* GetName() const;
        const char* ParentGroupName() const;
        const VkShaderStageFlags& GetStages() const noexcept;
        const VkDescriptorType& GetType() const noexcept;
        const VkImageCreateInfo& ImageInfo() const noexcept;
        const VkSamplerCreateInfo& SamplerInfo() const noexcept;
        const VkBufferViewCreateInfo& BufferViewInfo() const noexcept;
        void GetMembers(size_t* num_members, ShaderResourceSubObject* dest_objects) const noexcept;

        void SetMemoryRequired(size_t amt);
        void SetStages(VkShaderStageFlags stages);
        void SetType(VkDescriptorType _type);
        void SetSizeClass(size_class _size_class);
        void SetName(const char* name);
        void SetParentGroupName(const char* parent_group_name);
        void SetMembers(const size_t num_members, ShaderResourceSubObject* src_objects);
        void SetFormat(VkFormat fmt);
        void SetImageInfo(VkImageCreateInfo image_info);
        void SetSamplerInfo(VkSamplerCreateInfo sampler_info);
        void SetBufferViewInfo(VkBufferViewCreateInfo buffer_info);

    private:
        std::unique_ptr<ShaderResourceImpl> impl;
    };

}

#endif //!ST_SHADER_RESOURCE_HPP