#include "DescriptorStructs.hpp"

namespace st {

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

    bool ShaderResource::operator==(const ShaderResource & other) {
        return (Name == other.Name) && (Binding == other.Binding) &&
            (ParentSet == other.ParentSet) && (Stages == other.Stages);
    }

    bool ShaderResource::operator<(const ShaderResource & other) {
        // Sort by parent set first, then binding loc within those sets.
        if (ParentSet != other.ParentSet) {
            return ParentSet < other.ParentSet;
        }
        else {
            return Binding < other.Binding;
        }
    }

    ShaderResource::operator VkDescriptorSetLayoutBinding() const {
        if (Type != VK_DESCRIPTOR_TYPE_MAX_ENUM && Type != VK_DESCRIPTOR_TYPE_RANGE_SIZE) {
            return VkDescriptorSetLayoutBinding{
                Binding, Type, 1, Stages, nullptr
            };
        }
        else {
            return VkDescriptorSetLayoutBinding{
                Binding, Type, 0, Stages, nullptr
            };
        }
    }

    std::string ShaderResource::GetType() const {
        return GetTypeString(Type);
    }

    void ShaderResource::SetType(std::string type_str) {
        Type = GetTypeFromString(type_str);
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