#pragma once
#ifndef ST_SHADER_RESOURCE_HPP
#define ST_SHADER_RESOURCE_HPP
#include "CommonInclude.hpp"
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

        std::string GetTypeStr() const;
        const uint32_t& GetBinding() const noexcept;
        const uint32_t& GetParentSetIdx() const noexcept;
        const VkShaderStageFlags& GetStages() const noexcept;
        const VkDescriptorType& GetType() const noexcept;
        const std::vector<ShaderResourceSubObject>& GetMembers() const noexcept;

        void SetTypeStr(std::string type_str);

    private:

        std::string name{ "" };
        size_class sizeClass{ size_class::Absolute };
        storage_class storageClass{ storage_class::Read };
        uint32_t binding{ std::numeric_limits<uint32_t>::max() };
        uint32_t parentSet{ std::numeric_limits<uint32_t>::max() };
        VkShaderStageFlags stages{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
        VkDescriptorType type{ VK_DESCRIPTOR_TYPE_MAX_ENUM };
        std::vector<ShaderResourceSubObject> members;
        VkFormat format{ VK_FORMAT_UNDEFINED };

    };

    DescriptorObject::storage_class StorageClassFromSPIRType(const spirv_cross::SPIRType & type);

}

#endif //!ST_SHADER_RESOURCE_HPP