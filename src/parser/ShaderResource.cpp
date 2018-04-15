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