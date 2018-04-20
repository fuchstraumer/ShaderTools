#pragma once
#ifndef ST_SHADER_RESOURCE_HPP
#define ST_SHADER_RESOURCE_HPP
#include "common/CommonInclude.hpp"
#include "parser/DescriptorStructs.hpp"

namespace st {

    class ShaderResourceImpl;

    class ST_API ShaderResource {
    public:

        ShaderResource();
        ~ShaderResource();
        
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
        
        bool operator==(const ShaderResource& other);
        bool operator<(const ShaderResource& other);
        explicit operator VkDescriptorSetLayoutBinding() const;
        
        const size_t& GetAmountOfMemoryRequired() const noexcept;
        const VkFormat& GetFormat() const noexcept;
        const char* GetName() const;
        const uint32_t& GetBinding() const noexcept;
        const VkShaderStageFlags& GetStages() const noexcept;
        const VkDescriptorType& GetType() const noexcept;
        void GetMembers(size_t* num_members, ShaderResourceSubObject* dest_objects) const noexcept;

        void SetStages(VkShaderStageFlags stages);
        void SetType(VkDescriptorType _type);
        void SetSizeClass(size_class _size_class);
        void SetStorageClass(storage_class _storage_class);
        void SetName(const char* name);
        void SetMembers(const size_t num_members, ShaderResourceSubObject* src_objects);
        void SetFormat(VkFormat fmt);
        void SetBinding(uint32_t binding);

    private:
        std::unique_ptr<ShaderResourceImpl> impl;
    };

}

#endif //!ST_SHADER_RESOURCE_HPP