#include "ResourceFormats.hpp"
#include "easyloggingpp/src/easylogging++.h"

namespace st {

    std::string GetDescriptorTypeString(const VkDescriptorType& type)
    {
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
        default:
            LOG(ERROR) << "Invalid VkDescriptorType enum value passed to enum-to-string method.";
            return std::string("InvalidType");
        }
    }

    VkDescriptorType GetDescriptorTypeFromString(const std::string& str)
    {
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
        else {
            LOG(ERROR) << "Invalid string passed to string-to-enum method!";
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    std::string GetShaderStageString(const VkShaderStageFlags& flag)
    {
        switch (flag) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return "Vertex";
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return "Fragment";
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return "Compute";
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            return "Geometry";
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return "TesselationControl";
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return "TesselationEvaluation";
        default:
            LOG(ERROR) << "Passed invalid shader stage flags to enum->string conversion method.";
            return "UNDEFINED_SHADER_STAGE";
        }
    }

    VkShaderStageFlags GetShaderStageFromString(const std::string& stage)
    {
        if (stage == "Vertex") {
            return VK_SHADER_STAGE_VERTEX_BIT;
        }
        else if (stage == "Fragment") {
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        else if (stage == "Compute") {
            return VK_SHADER_STAGE_COMPUTE_BIT;
        }
        else if (stage == "Geometry") {
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        else if (stage == "TesselationControl") {
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        }
        else if (stage == "TesselationEvaluation") {
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        }
        else {
            LOG(ERROR) << "Passed invalid string of " << stage << " to string->stage enum conversion method.";
            return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        }
    }

    std::string SPIR_TypeToString(const spirv_cross::SPIRType& stype)
    {
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
        case SPIRType::Struct:
            return "struct";
            break;
        case SPIRType::Sampler:
            result_str = "sampler";
            break;
        case SPIRType::Image:
            result_str = "image";
            break;
        case SPIRType::SampledImage:
            result_str = "sampledImage";
            break;
        default:
            LOG(ERROR) << "Unhandled or unsupported SPIRType in TypeToStr method.";
            return "INVALID_TYPE";
        }

        if (add_width)
        {
            result_str += std::to_string(stype.width);
        }

        if (stype.vecsize <= 1)
        {
            return result_str;
        }
        else
        {
            result_str += std::string("_vector");
            result_str += std::to_string(stype.vecsize);
            return result_str;
        }

    }

    spirv_cross::SPIRType::BaseType SPIR_BaseTypeEnumFromString(const std::string & str)
    {
        using namespace spirv_cross;
        if (str == std::string("float"))
        {
            return SPIRType::Float;
        }
        else if (str == std::string("int"))
        {
            return SPIRType::Int;
        }
        else if (str == std::string("int64"))
        {
            return SPIRType::Int64;
        }
        else if (str == std::string("uint"))
        {
            return SPIRType::UInt;
        }
        else if (str == std::string("uint64"))
        {
            return SPIRType::UInt64;
        }
        else if (str == std::string("double"))
        {
            return SPIRType::Double;
        }
        else if (str == std::string("bool"))
        {
            return SPIRType::Boolean;
        }
        else if (str == std::string("half"))
        {
            return SPIRType::Half;
        }
        else if (str == std::string("char"))
        {
            return SPIRType::Char;
        }
        else if (str == std::string("struct"))
        {
            return SPIRType::Struct;
        }
        else if (str == std::string("sampler"))
        {
            return SPIRType::Sampler;
        }
        else if (str == std::string("image"))
        {
            return SPIRType::Image;
        }
        else if (str == std::string("sampledImage"))
        {
            return SPIRType::SampledImage;
        }
        else
        {
            LOG(ERROR) << "Invalid type string " << str << " passed to spirv_cross::BaseType from string conversion method.";
            return SPIRType::Unknown;
        }
    }

    spirv_cross::SPIRType SPIR_TypeFromString(const std::string & str)
    {
        using namespace spirv_cross;
        SPIRType result;

        size_t idx = str.find_first_of('_');

        if (idx != std::string::npos)
        {
            std::string type_substr = str.substr(0, idx);
            result.basetype = SPIR_BaseTypeEnumFromString(type_substr); 
            // set vec size now.
            std::string vec_substr = str.substr(0, idx + 1);
            vec_substr.erase(0, 6);
            result.vecsize = static_cast<uint32_t>(std::stoi(vec_substr));
            return result;
        }
        else
        {
            result = SPIRType();
            result.basetype = SPIR_BaseTypeEnumFromString(str);
            return result;
        }
    }

    VkFormat VkFormatFromSPIRType(const spirv_cross::SPIRType & type)
    {
        using namespace spirv_cross;
        if (type.basetype == SPIRType::Struct || type.basetype == SPIRType::Sampler)
        {
            LOG(WARNING) << "Tried to convert a structure or SPIR sampler into a VkFormat enum value!";
            return VK_FORMAT_UNDEFINED;
        }
        else if (type.basetype == SPIRType::Image || type.basetype == SPIRType::SampledImage)
        {
            switch (type.image.format)
            {
            case spv::ImageFormatR8:
                return VK_FORMAT_R8_UNORM;
            case spv::ImageFormatR8Snorm:
                return VK_FORMAT_R8_SNORM;
            case spv::ImageFormatR8ui:
                return VK_FORMAT_R8_UINT;
            case spv::ImageFormatR8i:
                return VK_FORMAT_R8_SINT;
            case spv::ImageFormatRg8:
                return VK_FORMAT_R8G8_UNORM;
            case spv::ImageFormatRg8Snorm:
                return VK_FORMAT_R8G8_SNORM;
            case spv::ImageFormatRg8ui:
                return VK_FORMAT_R8G8_UINT;
            case spv::ImageFormatRg8i:
                return VK_FORMAT_R8G8_SINT;
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
            default:
                LOG(ERROR) << "Failed to convert an image format to a VkFormat enum.";
                return VK_FORMAT_UNDEFINED;
            }
        }
        else if (type.vecsize == 1)
        {
            if (type.width == 8)
            {
                switch (type.basetype)
                {
                case SPIRType::Int:
                    return VK_FORMAT_R8_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R8_UINT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 16)
            {
                switch (type.basetype)
                {
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
            else if (type.width == 32)
            {
                switch (type.basetype)
                {
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
            else if (type.width == 64)
            {
                switch (type.basetype)
                {
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
            else
            {
                LOG(ERROR) << "Invalid type width for conversion of SPIR-Type to VkFormat enum value!";
                return VK_FORMAT_UNDEFINED;
            }
        }
        else if (type.vecsize == 2)
{
            if (type.width == 8)
            {
                switch (type.basetype)
                {
                case SPIRType::Int:
                    return VK_FORMAT_R8G8_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R8G8_UINT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 16)
            {
                switch (type.basetype)
                {
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
            else if (type.width == 32)
            {
                switch (type.basetype)
                {
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
            else if (type.width == 64)
            {
                switch (type.basetype)
                {
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
            else
            {
                LOG(ERROR) << "Invalid type width for conversion of SPIR-Type to VkFormat enum value!";
                return VK_FORMAT_UNDEFINED;
            }
        }
        else if (type.vecsize == 3)
{
            if (type.width == 8)
            {
                switch (type.basetype)
                {
                case SPIRType::Int:
                    return VK_FORMAT_R8G8B8_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R8G8B8_UINT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 16)
            {
                switch (type.basetype)
                {
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
            else if (type.width == 32)
            {
                switch (type.basetype)
                {
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
            else if (type.width == 64)
            {
                switch (type.basetype)
                {
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
            else
            {
                LOG(ERROR) << "Invalid type width for conversion of SPIR-Type to VkFormat enum value!";
                return VK_FORMAT_UNDEFINED;
            }
        }
        else if (type.vecsize == 4)
{
            if (type.width == 8)
            {
                switch (type.basetype)
                {
                case SPIRType::Int:
                    return VK_FORMAT_R8G8B8A8_SINT;
                case SPIRType::UInt:
                    return VK_FORMAT_R8G8B8A8_UINT;
                default:
                    return VK_FORMAT_UNDEFINED;
                }
            }
            else if (type.width == 16)
            {
                switch (type.basetype)
                {
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
            else if (type.width == 32)
            {
                switch (type.basetype)
                {
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
            else if (type.width == 64)
            {
                switch (type.basetype)
                {
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
            else
            {
                LOG(ERROR) << "Invalid type width for conversion to a VkFormat enum";
                return VK_FORMAT_UNDEFINED;
            }
        }
        else
{
            LOG(ERROR) << "Vector size in vertex input attributes isn't explicitly supported for parsing from SPIRType->VkFormat";
            return VK_FORMAT_UNDEFINED;
        }
    }

    VkFormat VkFormatFromString(const std::string & string)
    {
        static const std::unordered_map<std::string, VkFormat> formats_map
        {
            { "r8", VK_FORMAT_R8_UNORM },
            { "r8_snorm", VK_FORMAT_R8_SNORM },
            { "r8ui", VK_FORMAT_R8_UINT },
            { "r8i", VK_FORMAT_R8_SINT },
            { "rg8", VK_FORMAT_R8G8_UNORM },
            { "rg8_snorm", VK_FORMAT_R8G8_SNORM },
            { "rg8ui", VK_FORMAT_R8G8_UINT },
            { "rg8i", VK_FORMAT_R8G8_SINT },
            { "rgb8", VK_FORMAT_R8G8B8_UNORM },
            { "rgb8_snorm", VK_FORMAT_R8G8B8_SNORM },
            { "rgb8ui", VK_FORMAT_R8G8B8_UINT },
            { "rgb8i", VK_FORMAT_R8G8B8_SINT },
            { "rgba8", VK_FORMAT_R8G8B8A8_UNORM },
            { "rgba8_snorm", VK_FORMAT_R8G8B8A8_SNORM },
            { "rbga8ui", VK_FORMAT_R8G8B8A8_UINT },
            { "rgba8i", VK_FORMAT_R8G8B8A8_SINT },
            { "r16", VK_FORMAT_R16_UNORM },
            { "r16_snorm", VK_FORMAT_R16_SNORM },
            { "r16_scaled", VK_FORMAT_R16_USCALED },
            { "r16_uscaled", VK_FORMAT_R16_SSCALED },
            { "r16ui", VK_FORMAT_R16_UINT },
            { "r16i", VK_FORMAT_R16_SINT },
            { "r16f", VK_FORMAT_R16_SFLOAT },
            { "rg16", VK_FORMAT_R16G16_UNORM },
            { "rg16_snorm", VK_FORMAT_R16G16_SNORM },
            { "rg16_scaled", VK_FORMAT_R16G16_USCALED },
            { "rg16_sscaled", VK_FORMAT_R16G16_SSCALED },
            { "rg16ui", VK_FORMAT_R16G16_UINT },
            { "rg16i", VK_FORMAT_R16G16_SINT },
            { "rg16f", VK_FORMAT_R16G16_SFLOAT },
            { "rgb10_a2", VK_FORMAT_A2R10G10B10_UNORM_PACK32 },
            { "rgb10_a2ui", VK_FORMAT_A2R10G10B10_UINT_PACK32 },
            { "rgb10_a2i", VK_FORMAT_A2R10G10B10_SINT_PACK32 },
            { "rgb10_a2_snorm", VK_FORMAT_A2R10G10B10_SNORM_PACK32 },
            { "r11f_g11f_b10f", VK_FORMAT_B10G11R11_UFLOAT_PACK32 },
            { "rgb16", VK_FORMAT_R16G16B16_UNORM },
            { "rgb16_snorm", VK_FORMAT_R16G16B16_SNORM },
            { "rgb16_scaled", VK_FORMAT_R16G16B16_USCALED },
            { "rgb16_sscaled", VK_FORMAT_R16G16B16_SSCALED },
            { "rgb16ui", VK_FORMAT_R16G16B16_UINT },
            { "rgb16i", VK_FORMAT_R16G16B16_SINT },
            { "rgb16f", VK_FORMAT_R16G16B16_SFLOAT },
            { "rgba16", VK_FORMAT_R16G16B16A16_UNORM },
            { "rgba16_snorm", VK_FORMAT_R16G16B16A16_SNORM },
            { "rgba16_scaled", VK_FORMAT_R16G16B16A16_USCALED },
            { "rgba16_sscaled", VK_FORMAT_R16G16B16A16_SSCALED },
            { "rgba16ui", VK_FORMAT_R16G16B16A16_UINT },
            { "rgba16i", VK_FORMAT_R16G16B16A16_SINT },
            { "rgba16f", VK_FORMAT_R16G16B16A16_SFLOAT },
            { "r32ui", VK_FORMAT_R32_UINT },
            { "r32i", VK_FORMAT_R32_SINT },
            { "r32f", VK_FORMAT_R32_SFLOAT },
            { "rg32ui", VK_FORMAT_R32G32_UINT },
            { "rg32i", VK_FORMAT_R32G32_SINT },
            { "rg32f", VK_FORMAT_R32G32_SFLOAT },
            { "rgb32ui", VK_FORMAT_R32G32B32_UINT },
            { "rgb32i", VK_FORMAT_R32G32B32_SINT },
            { "rgb32f", VK_FORMAT_R32G32B32_SFLOAT },
            { "rgba32ui", VK_FORMAT_R32G32B32A32_UINT },
            { "rgba32i", VK_FORMAT_R32G32B32A32_SINT },
            { "rgba32f", VK_FORMAT_R32G32B32A32_SFLOAT }
        };
        auto iter = formats_map.find(string);
        if (iter != formats_map.cend())
        {
            return iter->second;
        }
        else
        {
            return VK_FORMAT_UNDEFINED;
        }
    }

    std::string VkFormatEnumToString(const VkFormat& fmt)
    {
        static const std::unordered_map<VkFormat, std::string> str_to_format_map
        {
            { VK_FORMAT_R8_UNORM, "r8" },
            { VK_FORMAT_R8_SNORM, "r8_snorm" },
            { VK_FORMAT_R8_UINT, "r8ui" },
            { VK_FORMAT_R8_SINT, "r8i" },
            { VK_FORMAT_R8G8_UNORM, "rg8" },
            { VK_FORMAT_R8G8_SNORM, "rg8_snorm" },
            { VK_FORMAT_R8G8_UINT, "rg8ui" },
            { VK_FORMAT_R8G8_SINT, "rg8i" },
            { VK_FORMAT_R8G8B8_UNORM, "rgb8" },
            { VK_FORMAT_R8G8B8_SNORM, "rgb8_snorm" },
            { VK_FORMAT_R8G8B8_UINT, "rgb8ui" },
            { VK_FORMAT_R8G8B8_SINT, "rgb8i" },
            { VK_FORMAT_R8G8B8A8_UNORM, "rgba8" },
            { VK_FORMAT_R8G8B8A8_SNORM, "rgba8_snorm" },
            { VK_FORMAT_R8G8B8A8_UINT, "rgba8ui" },
            { VK_FORMAT_R8G8B8A8_SINT, "rgba8i" },
            { VK_FORMAT_R16_UNORM, "r16" },
            { VK_FORMAT_R16_SNORM, "r16_snorm" },
            { VK_FORMAT_R16_USCALED, "r16_scaled" },
            { VK_FORMAT_R16_SSCALED, "r16_sscaled" },
            { VK_FORMAT_R16_UINT, "r16ui" },
            { VK_FORMAT_R16_SINT, "r16i" },
            { VK_FORMAT_R16_SFLOAT, "r16f" },
            { VK_FORMAT_R16G16_UNORM, "rg16" },
            { VK_FORMAT_R16G16_SNORM, "rg16_snorm" },
            { VK_FORMAT_R16G16_USCALED, "rg16_scaled" },
            { VK_FORMAT_R16G16_SSCALED, "rg16_sscaled" },
            { VK_FORMAT_R16G16_UINT, "rg16ui" },
            { VK_FORMAT_R16G16_SINT, "rg16i" },
            { VK_FORMAT_R16G16_SFLOAT, "rg16f" },
            { VK_FORMAT_R16G16B16_UNORM, "rgb16" },
            { VK_FORMAT_R16G16B16_SNORM, "rgb16_snorm" },
            { VK_FORMAT_R16G16B16_USCALED, "rgb16_scaled" },
            { VK_FORMAT_R16G16B16_SSCALED, "rgb16_sscaled" }, 
            { VK_FORMAT_R16G16B16_UINT, "rgb16ui" },
            { VK_FORMAT_R16G16B16_SINT, "rgb16i" },
            { VK_FORMAT_R16G16B16_SFLOAT, "rgb16f" },
            { VK_FORMAT_R16G16B16A16_UINT, "rgba16ui" },
            { VK_FORMAT_R16G16B16A16_SINT, "rgba16i" },
            { VK_FORMAT_R16G16B16A16_SFLOAT, "rgba16f" },
            { VK_FORMAT_R32_UINT, "r32ui" },
            { VK_FORMAT_R32_SINT, "r32i" },
            { VK_FORMAT_R32_SFLOAT, "r32f" },
            { VK_FORMAT_A2R10G10B10_UNORM_PACK32, "rgb10_a2" },
            { VK_FORMAT_A2B10G10R10_SNORM_PACK32, "rgba10_a2ui_snorm" },
            { VK_FORMAT_A2B10G10R10_UINT_PACK32, "rgba10_a2ui" },
            { VK_FORMAT_A2B10G10R10_SINT_PACK32, "rgba10_a2i" },
            { VK_FORMAT_B10G11R11_UFLOAT_PACK32, "r11f_g11f_b10f" },
            { VK_FORMAT_R32G32_UINT, "rg32ui" },
            { VK_FORMAT_R32G32_SINT, "rg32ui" },
            { VK_FORMAT_R32G32_SFLOAT, "rg32f" },
            { VK_FORMAT_R32G32B32_UINT, "rgb32ui" },
            { VK_FORMAT_R32G32B32_SINT, "rgb32i" },
            { VK_FORMAT_R32G32B32_SFLOAT, "rgb32f" },
            { VK_FORMAT_R32G32B32A32_UINT, "rgba32ui" },
            { VK_FORMAT_R32G32B32A32_SINT, "rgba32i" },
            { VK_FORMAT_R32G32B32A32_SFLOAT, "rgba32f" }
        };
        auto iter = str_to_format_map.find(fmt);
        if (iter != str_to_format_map.cend()) {
            return iter->second;
        }
        else {
            return "";
        }
    }

    size_t VkFormatSize(const VkFormat& fmt)
    {
        static const std::unordered_map<VkFormat, size_t> format_sizes
        {
            { VK_FORMAT_R8_UNORM, 1 * sizeof(uint8_t) },
            { VK_FORMAT_R8_SNORM, 1 * sizeof(uint8_t) },
            { VK_FORMAT_R8_UINT, 1 * sizeof(uint8_t) },
            { VK_FORMAT_R8_SINT, 1 * sizeof(int8_t) },
            { VK_FORMAT_R8G8_UNORM, 2 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8_SNORM, 2 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8_UINT, 2 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8_SINT, 2 * sizeof(int8_t) },
            { VK_FORMAT_R8G8B8_UNORM, 3 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8B8_SNORM, 3 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8B8_UINT, 3 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8B8_SINT, 3 * sizeof(int8_t) },
            { VK_FORMAT_R8G8B8A8_UNORM, 4 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8B8A8_SNORM, 4 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8B8A8_UINT, 4 * sizeof(uint8_t) },
            { VK_FORMAT_R8G8B8A8_SINT, 4 * sizeof(int8_t) },
            { VK_FORMAT_R16_UNORM, 1 * sizeof(uint16_t) },
            { VK_FORMAT_R16_SNORM, 1 * sizeof(uint16_t) },
            { VK_FORMAT_R16_USCALED, 1 * sizeof(uint16_t) },
            { VK_FORMAT_R16_SSCALED, 1 * sizeof(uint16_t) },
            { VK_FORMAT_R16_UINT, 1 * sizeof(uint16_t) },
            { VK_FORMAT_R16_SINT, 1 * sizeof(int16_t) },
            { VK_FORMAT_R16_SFLOAT, 1 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16_UNORM, 2 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16_SNORM, 2 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16_USCALED, 2 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16_SSCALED, 2 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16_UINT, 2 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16_SINT, 2 * sizeof(int16_t) },
            { VK_FORMAT_R16G16_SFLOAT, 2 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16_UNORM, 3 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16_SNORM, 3 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16_USCALED, 3 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16_SSCALED, 3 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16_UINT, 3 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16_SINT, 3 * sizeof(int16_t) },
            { VK_FORMAT_R16G16B16_SFLOAT, 3 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16A16_UNORM, 4 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16A16_SNORM, 4 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16A16_USCALED, 4 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16A16_SSCALED, 4 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16A16_UINT, 4 * sizeof(uint16_t) },
            { VK_FORMAT_R16G16B16A16_SINT, 4 * sizeof(int16_t) },
            { VK_FORMAT_R16G16B16A16_SFLOAT, 4 * sizeof(uint16_t) },
            { VK_FORMAT_A2B10G10R10_UNORM_PACK32, 1 * sizeof(uint32_t) },
            { VK_FORMAT_A2B10G10R10_UINT_PACK32, 1 * sizeof(uint32_t) },
            { VK_FORMAT_A2B10G10R10_SINT_PACK32, 1 * sizeof(int32_t) },
            { VK_FORMAT_B10G11R11_UFLOAT_PACK32, 1 * sizeof(uint32_t) },
            { VK_FORMAT_R32_UINT, 1 * sizeof(uint32_t) },
            { VK_FORMAT_R32_SINT, 1 * sizeof(int32_t) },
            { VK_FORMAT_R32_SFLOAT, 1 * sizeof(float) },
            { VK_FORMAT_R32G32_UINT, 2 * sizeof(uint32_t) },
            { VK_FORMAT_R32G32_SINT, 2 * sizeof(int32_t) },
            { VK_FORMAT_R32G32_SFLOAT, 2 * sizeof(float) },
            { VK_FORMAT_R32G32B32_UINT, 3 * sizeof(uint32_t) },
            { VK_FORMAT_R32G32B32_SINT, 3 * sizeof(int32_t) },
            { VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float) },
            { VK_FORMAT_R32G32B32A32_UINT, 4 * sizeof(uint32_t) },
            { VK_FORMAT_R32G32B32A32_SINT, 4 * sizeof(int16_t) },
            { VK_FORMAT_R32G32B32A32_SFLOAT, 4 * sizeof(float) }
        };

        auto iter = format_sizes.find(fmt);
        if (iter != format_sizes.cend())
        {
            return iter->second;
        }
        else
        {
            LOG(WARNING) << "Couldn't find VkFormat value " << std::to_string(fmt) << " in VkFormat->MemoryFootprint map! Returning invalid value.";
            return std::numeric_limits<size_t>::max();
        }
    }
}
