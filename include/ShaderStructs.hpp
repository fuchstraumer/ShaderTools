#pragma once
#ifndef SHADER_TOOLS_STRUCTS_HPP
#define SHADER_TOOLS_STRUCTS_HPP
#include "CommonInclude.hpp"
#include <vector>
#include <map>

namespace st {

    inline std::string GetTypeString(const VkDescriptorType& type) {
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
        default:
            throw std::domain_error("Invalid VkDescriptorType enum value passed to enum-to-string method.");
        }
    }

    inline VkDescriptorType GetTypeFromString(const std::string& str) {
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
        else {
            throw std::domain_error("Invalid string passed to string-to-enum method!");
        }
    }

    struct ShaderDataObject {
        std::string Name;
        uint32_t Size, Offset;
    };

    struct DescriptorObject {
        std::string Name;
        uint32_t Binding, ParentSet;
        VkShaderStageFlags Stages;
        VkDescriptorType Type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        std::vector<ShaderDataObject> Members;

        bool operator==(const DescriptorObject& other) {
            return (Name == other.Name) && (Binding == other.Binding) &&
                   (ParentSet == other.ParentSet) && (Stages == other.Stages);
        }
        bool operator<(const DescriptorObject& other) {
            // Sort by parent set first, then binding loc within those sets.
            if(ParentSet != other.ParentSet) {
                return ParentSet < other.ParentSet;
            }
            else {
                return Binding < other.Binding;
            }
        }

        explicit operator VkDescriptorSetLayoutBinding() const {
            return VkDescriptorSetLayoutBinding {
                Binding, Type, 1, Stages, nullptr
            };
        }

        std::string GetType() const {
            return GetTypeString(Type);
        }

        void SetType(std::string type_str) {
            Type = GetTypeFromString(type_str);
        }

    };

    struct DescriptorSetInfo {
        uint32_t Index = std::numeric_limits<uint32_t>::max();
        std::vector<DescriptorObject> Members = std::vector<DescriptorObject>();
    };

    struct PushConstantInfo {
        VkShaderStageFlags Stages;
        std::string Name;
        std::vector<ShaderDataObject> Members;
        uint32_t Offset;
        explicit operator VkPushConstantRange() const noexcept {
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
    };

    template<typename T>
    void to_json(nlohmann::json& j, const T& obj);
    template<typename T>
    void from_json(const nlohmann::json& j, T& obj);

}

namespace meta {
    
    template<>
    inline auto registerMembers<st::ShaderDataObject>() {
        using namespace st;
        return members( 
            member("Name", &ShaderDataObject::Name),
            member("Size", &ShaderDataObject::Size),
            member("Offset", &ShaderDataObject::Offset)
        );
    }

    template<>
    inline auto registerMembers<st::DescriptorObject>() {
        using namespace st;
        return members(
            member("Name", &DescriptorObject::Name),
            member("Binding", &DescriptorObject::Binding),
            member("ParentSet", &DescriptorObject::ParentSet),
            member("Type", &DescriptorObject::GetType, &DescriptorObject::SetType),
            member("Stages", &DescriptorObject::Stages),
            member("Members", &DescriptorObject::Members)
        );
    }

    template<>
    inline auto registerMembers<st::DescriptorSetInfo>() {
        using namespace st;
        return members(
            member("Index", &DescriptorSetInfo::Index),
            member("Objects", &DescriptorSetInfo::Members)
        );
    }

    template<>
    inline auto registerMembers<st::PushConstantInfo>() {
        using namespace st;
        return members(
            member("Name", &PushConstantInfo::Name),
            member("Stages", &PushConstantInfo::Stages),
            member("Members", &PushConstantInfo::Members)
        );
    }

    template<typename Class, typename = std::enable_if_t<meta::isRegistered<Class>()>>
    nlohmann::json serialize(const Class& obj) {
        nlohmann::json result;
        meta::doForAllMembers<Class>([&obj, &result](auto & member) {
            auto& value_name = result[member.getName()];
            if(member.canGetConstRef()) {
                value_name = member.get(obj);
            }
            else if(member.hasGetter()) {
                value_name = member.getCopy(obj);
            }
        }
        );
        return result;
    }

    template<typename Class, typename = std::enable_if_t<!meta::isRegistered<Class>()>, typename = void>
    nlohmann::json serialize(const Class& obj);

    template<typename T>
    nlohmann::json serialize(const std::vector<T>& _obj) {
        nlohmann::json result;
        size_t i = 0;
        for(auto& elem : _obj) {
            result[i] = elem;
            ++i;
        }
        return result;
    }

    template<typename Class, typename = std::enable_if_t<meta::isRegistered<Class>()>>
    void deserialize(Class& obj, const nlohmann::json& j) {
        if(j.is_object()) {
            meta::doForAllMembers<Class> (
                [&obj, &j](auto& member) {
                    using member_t = meta::get_member_type<decltype(member)>;
                    if(j.count(member.getName()) != 0) {
                        auto& obj_name = j[member.getName()];
                        if(!obj_name.is_null()) {
                            
                            if(member.hasSetter()) {
                                member.set(obj, obj_name.template get<member_t>());
                            }
                            else if(member.canGetRef()) {
                                member.getRef(obj) = obj_name.template get<member_t>();
                            }
                            else {
                                throw std::runtime_error("Error: read-only objects cannot be de-serialized!");
                            }
                        }
                    }
                    else {
                        if(member.canGetRef()) {
                            member.getRef(obj) = member_t();
                        }
                    }
                }
            );
        }
    }

    template<typename T>
    void deserialize(std::vector<T>& _vector, const nlohmann::json& input) {
        _vector.reserve(input.size());
        for(auto& elem : input) {
            _vector.push_back(elem);
        }
    }

    template<typename Key, typename Value>
    void deserialize(std::map<Key, Value>& _map, const nlohmann::json& input) {
        for(auto iter = input.begin(); iter != input.end(); ++iter) {
            _map.emplace(iter.key(), iter.value());
        }
    }

}

template<typename T>
inline void st::to_json(nlohmann::json& j, const T& obj) {
    j = meta::serialize(obj);
}

template<typename T>
inline void st::from_json(const nlohmann::json& j, T& obj) {
    meta::deserialize(obj, j);
}


#endif //!SHADER_TOOLS_STRUCTS_HPP