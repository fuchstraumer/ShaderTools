#include "parser/ShaderResource.hpp"

namespace st {

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
                0, type, 1, stages, nullptr
            };
        }
        else {
            return VkDescriptorSetLayoutBinding{
                0, type, 0, stages, nullptr
            };
        }
    }

    std::string ShaderResource::GetName() const {
        return name;
    }

    const uint32_t & ShaderResource::GetBinding() const noexcept {
        return binding;
    }

    const VkShaderStageFlags & ShaderResource::GetStages() const noexcept
    {
        return stages;
    }

    const VkDescriptorType & ShaderResource::GetType() const noexcept
    {
        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }

    const std::vector<ShaderResourceSubObject>& ShaderResource::GetMembers() const noexcept
    {
        return members;
    }

    void ShaderResource::SetStages(VkShaderStageFlags stages)
    {
    }

    void ShaderResource::SetType(VkDescriptorType _type)
    {
    }

    void ShaderResource::SetSizeClass(size_class _size_class)
    {
    }

    void ShaderResource::SetStorageClass(storage_class _storage_class)
    {
    }

    void ShaderResource::SetName(std::string name)
    {
    }

    void ShaderResource::SetMembers(std::vector<ShaderResourceSubObject> _members)
    {
    }

    void ShaderResource::SetFormat(VkFormat fmt)
    {
    }

    void ShaderResource::SetBinding(uint32_t binding)
    {
    }

}