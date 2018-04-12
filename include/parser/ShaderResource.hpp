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
        std::string GetTypeStr() const;
        const uint32_t& GetBinding() const noexcept;
        const uint32_t& GetParentSet() const noexcept;
        const VkShaderStageFlags& GetStages() const noexcept;
        const VkDescriptorType& GetType() const noexcept;
        const std::vector<ShaderResourceSubObject>& GetMembers() const noexcept;

        void SetBinding(uint32_t _binding);
        void SetParentSet(uint32_t parent_set);
        void SetStages(VkShaderStageFlags stages);
        void SetType(VkDescriptorType _type);
        void SetSizeClass(size_class _size_class);
        void SetStorageClass(storage_class _storage_class);
        void SetName(std::string name);
        void SetMembers(std::vector<ShaderResourceSubObject> _members);
        void SetFormat(VkFormat fmt);

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


    ShaderResource::storage_class StorageClassFromSPIRType(const spirv_cross::SPIRType & type) {
        using namespace spirv_cross;
        if (type.basetype == SPIRType::SampledImage || type.basetype == SPIRType::Sampler || type.basetype == SPIRType::Struct || type.basetype == SPIRType::AtomicCounter) {
            return ShaderResource::storage_class::Read;
        }
        else {
            switch (type.image.access) {
            case spv::AccessQualifier::AccessQualifierReadOnly:
                return ShaderResource::storage_class::Read;
            case spv::AccessQualifier::AccessQualifierWriteOnly:
                return ShaderResource::storage_class::Write;
            case spv::AccessQualifier::AccessQualifierReadWrite:
                return ShaderResource::storage_class::ReadWrite;
            case spv::AccessQualifier::AccessQualifierMax:
                // Usually happens for storage images.
                return ShaderResource::storage_class::ReadWrite;
            default:
                throw std::domain_error("SPIRType somehow has invalid access qualifier enum value!");
            }
        }
    }


    ShaderResource::operator VkDescriptorSetLayoutBinding() const {
        if (type != VK_DESCRIPTOR_TYPE_MAX_ENUM && type != VK_DESCRIPTOR_TYPE_RANGE_SIZE) {
            return VkDescriptorSetLayoutBinding{
                binding, type, 1, stages, nullptr
            };
        }
        else {
            return VkDescriptorSetLayoutBinding{
                binding, type, 0, stages, nullptr
            };
        }
    }

}

#endif //!ST_SHADER_RESOURCE_HPP