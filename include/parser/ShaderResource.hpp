#pragma once
#ifndef ST_SHADER_RESOURCE_HPP
#define ST_SHADER_RESOURCE_HPP
#include "common/CommonInclude.hpp"
#include "DescriptorStructs.hpp"

namespace st {

    class ShaderResource {
    public:
        
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

        std::string GetName() const;
        const uint32_t& GetBinding() const noexcept;
        const VkShaderStageFlags& GetStages() const noexcept;
        const VkDescriptorType& GetType() const noexcept;
        const std::vector<ShaderResourceSubObject>& GetMembers() const noexcept;

        void SetStages(VkShaderStageFlags stages);
        void SetType(VkDescriptorType _type);
        void SetSizeClass(size_class _size_class);
        void SetStorageClass(storage_class _storage_class);
        void SetName(std::string name);
        void SetMembers(std::vector<ShaderResourceSubObject> _members);
        void SetFormat(VkFormat fmt);
        void SetBinding(uint32_t binding);

    private:

        uint32_t binding{ 0 };
        std::string name{ "" };
        size_class sizeClass{ size_class::Absolute };
        storage_class storageClass{ storage_class::Read };
        VkShaderStageFlags stages{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
        VkDescriptorType type{ VK_DESCRIPTOR_TYPE_MAX_ENUM };
        std::vector<ShaderResourceSubObject> members;
        VkFormat format{ VK_FORMAT_UNDEFINED };

    };

}

#endif //!ST_SHADER_RESOURCE_HPP