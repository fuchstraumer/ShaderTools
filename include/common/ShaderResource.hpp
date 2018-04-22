#pragma once
#ifndef ST_SHADER_RESOURCE_HPP
#define ST_SHADER_RESOURCE_HPP
#include "common/CommonInclude.hpp"
#include "../parser/DescriptorStructs.hpp"

namespace st {

    class ShaderResourceImpl;

    enum class storage_class {
        Read,
        Write,
        ReadWrite
    };

    enum class size_class {
        Absolute,
        SwapchainRelative,
        ViewportRelative
    };

    class ST_API ShaderResource {
    public:

        ShaderResource();
        ShaderResource(uint32_t parent_idx, uint32_t binding_idx);
        ~ShaderResource();
        ShaderResource(const ShaderResource& other) noexcept;
        ShaderResource(ShaderResource&& other) noexcept;

        ShaderResource& operator=(const ShaderResource& other) noexcept;
        ShaderResource& operator=(ShaderResource&& other) noexcept;
        
        bool operator==(const ShaderResource& other) const noexcept;
        bool operator<(const ShaderResource& other) const noexcept;
        explicit operator VkDescriptorSetLayoutBinding() const;
        
        const size_t& GetAmountOfMemoryRequired() const noexcept;
        const VkFormat& GetFormat() const noexcept;
        const char* GetName() const;
        const uint32_t& GetParentIdx() const noexcept;
        const uint32_t& GetBinding() const noexcept;
        const VkShaderStageFlags& GetStages() const noexcept;
        const VkDescriptorType& GetType() const noexcept;
        void GetMembers(size_t* num_members, ShaderResourceSubObject* dest_objects) const noexcept;

        void SetMemoryRequired(size_t amt);
        void SetStages(VkShaderStageFlags stages);
        void SetType(VkDescriptorType _type);
        void SetSizeClass(size_class _size_class);
        void SetStorageClass(storage_class _storage_class);
        void SetName(const char* name);
        void SetMembers(const size_t num_members, ShaderResourceSubObject* src_objects);
        void SetFormat(VkFormat fmt);
        void SetParentIdx(uint32_t parent_idx);
        void SetBinding(uint32_t binding);

    private:
        std::unique_ptr<ShaderResourceImpl> impl;
    };

}

#endif //!ST_SHADER_RESOURCE_HPP