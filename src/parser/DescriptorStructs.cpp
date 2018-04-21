#include "parser/DescriptorStructs.hpp"

namespace st {

     std::string StageFlagToStr(const VkShaderStageFlags & flag) {
        switch (flag) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return "vertex-shader";
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return "fragment-shader";
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return "compute-shader";
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            return "geometry-shader";
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return "tesselation-control-shader";
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return "tesselation-evaluation-bit";
        default:
            return "UNDEFINED_SHADER_STAGE";
        }
    }

    VkShaderStageFlags StrToStageFlags(const std::string& stage) {
        if (stage == "vertex-shader") {
            return VK_SHADER_STAGE_VERTEX_BIT;
        }
        else if (stage == "fragment-shader") {
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        else if (stage == "compute-shader") {
            return VK_SHADER_STAGE_COMPUTE_BIT;
        }
        else if (stage == "geometry-shader") {
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        else if (stage == "tesselation-control-shader") {
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        }
        else if (stage == "tesselation-evaluation-shader") {
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        }
        else {
            throw std::domain_error("Invalid shader stage string.");
        }
    }

    std::string TypeToStr(const spirv_cross::SPIRType & stype) {
        using namespace spirv_cross;
        std::string result_str;
        bool add_width = true;

        switch (stype.basetype) {
        case SPIRType::Boolean:
            result_str = "bool";
            break;
        case SPIRType::Char:
            result_str = "char";
            break;
        case SPIRType::AtomicCounter:
            result_str = "atomic_counter";
            break;
        case SPIRType::Half:
            result_str = "half";
            break;
        case SPIRType::Float:
            result_str = "float";
            break;
        case SPIRType::Int:
            result_str = "int";
            break;
        case SPIRType::Int64:
            result_str = "int64";
            add_width = false;
            break;
        case SPIRType::UInt:
            result_str = "uint";
            break;
        case SPIRType::UInt64:
            result_str = "uint64";
            add_width = false;
            break;
        case SPIRType::Double:
            result_str = "double";
            break;
        default:
            throw std::domain_error("Encountered unhandled SPIRType in TypeToStr method.");
        }

        if (add_width) {
            result_str += std::to_string(stype.width);
        }

        if (stype.vecsize <= 1) {
            return result_str;
        }
        else {
            result_str += std::string("_vector");
            result_str += std::to_string(stype.vecsize);
            return result_str;
        }

    }

    spirv_cross::SPIRType::BaseType BaseTypeEnumFromStr(const std::string & str) {
        using namespace spirv_cross;
        if (str == std::string("float")) {
            return SPIRType::Float;
        }
        else if (str == std::string("int")) {
            return SPIRType::Int;
        }
        else if (str == std::string("int64")) {
            return SPIRType::Int64;
        }
        else if (str == std::string("uint")) {
            return SPIRType::UInt;
        }
        else if (str == std::string("uint64")) {
            return SPIRType::UInt64;
        }
        else if (str == std::string("double")) {
            return SPIRType::Double;
        }
        else if (str == std::string("bool")) {
            return SPIRType::Boolean;
        }
        else if (str == std::string("half")) {
            return SPIRType::Half;
        }
        else if (str == std::string("char")) {
            return SPIRType::Char;
        }
        else if (str == std::string("atomic_counter")) {
            return SPIRType::AtomicCounter;
        }
        else {
            throw std::domain_error("Invalid type string passed to BaseType retrieval method.");
        }
    }

    spirv_cross::SPIRType TypeFromStr(const std::string & str) {
        using namespace spirv_cross;
        SPIRType result;

        size_t idx = str.find_first_of('_');

        if (idx != std::string::npos) {
            std::string type_substr = str.substr(0, idx);
            result.basetype = BaseTypeEnumFromStr(type_substr);
        }
        else {
            result = SPIRType();
            result.basetype = BaseTypeEnumFromStr(str);
            return result;
        }

        // set vec size now.
        std::string vec_substr = str.substr(0, idx + 1);
        vec_substr.erase(0, 6);
        result.vecsize = static_cast<uint32_t>(std::stoi(vec_substr));
        return result;
    }

    VkFormat FormatFromSPIRType(const spirv_cross::SPIRType & type) {
        using namespace spirv_cross;
        if (type.basetype == SPIRType::Struct || type.basetype == SPIRType::Sampler) {
            // no real format info for structs to be extracted
            return VK_FORMAT_UNDEFINED;
        }
        else if (type.basetype == SPIRType::Image || type.basetype == SPIRType::SampledImage) {
            switch (type.image.format) {
            case spv::ImageFormatR8ui:
                return VK_FORMAT_R8_UINT;
            case spv::ImageFormat::ImageFormatR8i:
                return VK_FORMAT_R8_SINT;
            case spv::ImageFormatRg8ui:
                return VK_FORMAT_R8G8_UINT;
            case spv::ImageFormatRg8i:
                return VK_FORMAT_R8G8_SINT;
            case spv::ImageFormatRgba32f:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case spv::ImageFormatRgba16f:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case spv::ImageFormatR32f:
                return VK_FORMAT_R32_SFLOAT;
            case spv::ImageFormatRg32f:
                return VK_FORMAT_R32G32_SFLOAT;
            case spv::ImageFormatR16f:
                return VK_FORMAT_R16_SFLOAT;
            case spv::ImageFormatRgba32i:
                return VK_FORMAT_R32G32B32A32_SINT;
            case spv::ImageFormatRgba32ui:
                return VK_FORMAT_R32G32B32A32_UINT;
            case spv::ImageFormatRgba8i:
                return VK_FORMAT_R8G8B8A8_SINT;
            case spv::ImageFormatRgba8ui:
                return VK_FORMAT_R8G8B8A8_UINT;
            case spv::ImageFormatRgba8:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case spv::ImageFormatRgba8Snorm:
                return VK_FORMAT_R8G8B8A8_SNORM;
            case spv::ImageFormatR32i:
                return VK_FORMAT_R32_SINT;
            case spv::ImageFormatR32ui:
                return VK_FORMAT_R32_UINT;
            case spv::ImageFormatRg32i:
                return VK_FORMAT_R32G32_SINT;
            case spv::ImageFormatRg32ui:
                return VK_FORMAT_R32G32_UINT;
            default:
                throw std::runtime_error("Failed to match SPV image type to VkFormat enum.");
            }
        }
        else if (type.vecsize == 1) {
            if (type.width == 8) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R8_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R8_UINT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 16) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R16_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R16_UINT;
                case SPIRType::Float:
                    return VK_FORMAT_R16_SFLOAT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 32) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R32_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R32_UINT;
                case SPIRType::Float:
                    return VK_FORMAT_R32_SFLOAT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 64) {
                switch (type.basetype) {
                    case SPIRType::Int64:
                        return VK_FORMAT_R64_SINT;
                    case SPIRType::UInt64:
                        return VK_FORMAT_R64_UINT;
                    case SPIRType::Double:
                        return VK_FORMAT_R64_SFLOAT;
                    default:
                        return VK_FORMAT_UNDEFINED;
                }
            }
            else {
                throw std::domain_error("Invalid type width for conversion to a VkFormat enum.");
            }
        }
        else if (type.vecsize == 2) {
            if (type.width == 8) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R8G8_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R8G8_UINT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 16) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R16G16_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R16G16_UINT;
                case SPIRType::Float:
                    return VK_FORMAT_R16G16_SFLOAT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 32) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R32G32_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R32G32_UINT;
                case SPIRType::Float:
                    return VK_FORMAT_R32G32_SFLOAT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 64) {
                switch (type.basetype) {
                    case SPIRType::Int64:
                        return VK_FORMAT_R64G64_SINT;
                    case SPIRType::UInt64:
                        return VK_FORMAT_R64G64_UINT;
                    case SPIRType::Double:
                        return VK_FORMAT_R64G64_SFLOAT;
                    default:
                        return VK_FORMAT_UNDEFINED;
                }
            }
            else {
                throw std::domain_error("Invalid type width for conversion to a VkFormat enum.");
            }
        }
        else if (type.vecsize == 3) {
            if (type.width == 8) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R8G8B8_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R8G8B8_UINT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 16) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R16G16B16_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R16G16B16_UINT;
                case SPIRType::Float:
                    return VK_FORMAT_R16G16B16_SFLOAT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 32) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R32G32B32_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R32G32B32_UINT;
                case SPIRType::Float:
                    return VK_FORMAT_R32G32B32_SFLOAT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 64) {
                switch (type.basetype) {
                    case SPIRType::Int64:
                        return VK_FORMAT_R64G64B64_SINT;
                    case SPIRType::UInt64:
                        return VK_FORMAT_R64G64B64_UINT;
                    case SPIRType::Double:
                        return VK_FORMAT_R64G64B64_SFLOAT;
                    default:
                        return VK_FORMAT_UNDEFINED;
                }
            }
            else {
                throw std::domain_error("Invalid type width for conversion to a VkFormat enum");
            }
        }
        else if (type.vecsize == 4) {
            if (type.width == 8) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R8G8B8A8_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R8G8B8A8_UINT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 16) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R16G16B16A16_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R16G16B16A16_UINT;
                case SPIRType::Float:
                    return VK_FORMAT_R16G16B16A16_SFLOAT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 32) {
                switch (type.basetype) {
                case SPIRType::Int:
                    return VK_FORMAT_R32G32B32A32_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R32G32B32A32_UINT;
                case SPIRType::Float:
                    return VK_FORMAT_R32G32B32A32_SFLOAT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 64) {
                switch (type.basetype) {
                    case SPIRType::Int64:
                        return VK_FORMAT_R64G64B64A64_SINT;
                    case SPIRType::UInt64:
                        return VK_FORMAT_R64G64B64A64_UINT;
                    case SPIRType::Double:
                        return VK_FORMAT_R64G64B64A64_SFLOAT;
                    default:
                        return VK_FORMAT_UNDEFINED;
                }
            }
            else {
                throw std::domain_error("Invalid type width for conversion to a VkFormat enum.");
            }
        }
        else {
            throw std::domain_error("Vector size in vertex input attributes isn't supported for parsing from SPIRType->VkFormat");
        }
    }


    std::string VertexAttributeInfo::GetTypeStr() const {
        return TypeToStr(Type);
    }

    void VertexAttributeInfo::SetTypeWithStr(std::string str) {
        Type = TypeFromStr(str);
    }

    VertexAttributeInfo::operator VkVertexInputAttributeDescription() const noexcept {
        return VkVertexInputAttributeDescription{ Location, 0, FormatFromSPIRType(Type), Offset };
    }

    void StageAttributes::SetStageWithStr(std::string str) {
        Stage = StrToStageFlags(str);
    }

    std::string StageAttributes::GetStageStr() const {
        return StageFlagToStr(Stage);
    }

    std::string GetTypeString(const VkDescriptorType & type) {
        switch (type) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            return std::string("UniformBuffer");
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            return std::string("UniformBufferDynamic");
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return std::string("StorageBuffer");
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            return std::string("StorageBufferDynamic");
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            return std::string("CombinedImageSampler");
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            return std::string("SampledImage");
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            return std::string("StorageImage");
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            return std::string("InputAttachment");
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            return std::string("Sampler");
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            return std::string("UniformTexelBuffer");
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            return std::string("StorageTexelBuffer");
        case VK_DESCRIPTOR_TYPE_RANGE_SIZE:
            return std::string("NULL_TYPE");
        default:
            throw std::domain_error("Invalid VkDescriptorType enum value passed to enum-to-string method.");
        }
    }

    VkDescriptorType GetTypeFromString(const std::string & str) {
        if (str == std::string("UniformBuffer")) {
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        else if (str == std::string("UniformBufferDynamic")) {
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        }
        else if (str == std::string("StorageBuffer")) {
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        else if (str == std::string("StorageBufferDynamic")) {
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        }
        else if (str == std::string("CombinedImageSampler")) {
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
        else if (str == std::string("SampledImage")) {
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        }
        else if (str == std::string("StorageImage")) {
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        else if (str == std::string("InputAttachment")) {
            return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        }
        else if (str == std::string("Sampler")) {
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        }
        else if (str == std::string("UniformTexelBuffer")) {
            return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        }
        else if (str == std::string("StorageTexelBuffer")) {
            return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        }
        else if (str == std::string("NULL_TYPE")) {
            return VK_DESCRIPTOR_TYPE_RANGE_SIZE;
        }
        else {
            throw std::domain_error("Invalid string passed to string-to-enum method!");
        }
    }

    PushConstantInfo::operator VkPushConstantRange() const noexcept {
        VkPushConstantRange result;
        result.stageFlags = Stages;
        result.offset = Offset;
        uint32_t size = 0;
        for (auto& obj : Members) {
            size += obj.Size;
        }
        result.size = size;
        return result;
    }

}