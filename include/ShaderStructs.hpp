#pragma once
#ifndef SHADER_TOOLS_STRUCTS_HPP
#define SHADER_TOOLS_STRUCTS_HPP
#include "CommonInclude.hpp"
#include "spirv_cross.hpp"
#include "nlohmann/include/nlohmann/json.hpp"
#include "metastuff/include/Meta.h"
#include <vector>
#include <map>

namespace st {

    std::string GetTypeString(const VkDescriptorType& type);
    VkDescriptorType GetTypeFromString(const std::string& str);

    std::string StageFlagToStr(const VkShaderStageFlags& flag);
    VkShaderStageFlags StrToStageFlags(const std::string& str);

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

        bool operator==(const DescriptorObject& other);
        bool operator<(const DescriptorObject& other);

        explicit operator VkDescriptorSetLayoutBinding() const;
        std::string GetType() const;
        void SetType(std::string type_str);

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
        explicit operator VkPushConstantRange() const noexcept;
    };
        
    std::string TypeToStr(const spirv_cross::SPIRType& stype);
    spirv_cross::SPIRType::BaseType BaseTypeEnumFromStr(const std::string& str);
    spirv_cross::SPIRType TypeFromStr(const std::string& str);
    VkFormat FormatFromSPIRType(const spirv_cross::SPIRType& type);

    struct VertexAttributeInfo {
        std::string Name;
        spirv_cross::SPIRType Type;
        std::string GetTypeStr() const;
        void SetTypeWithStr(std::string str);
        explicit operator VkVertexInputAttributeDescription() const noexcept;
        uint32_t Location;
        uint32_t Binding;
        uint32_t Offset;
    };

    struct StageAttributes {
        std::vector<VertexAttributeInfo> InputAttributes;
        std::vector<VertexAttributeInfo> OutputAttributes;
        VkShaderStageFlags Stage;
        void SetStageWithStr(std::string str);
        std::string GetStageStr() const;
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

    template<>
    inline auto registerMembers<st::VertexAttributeInfo>() {
        using namespace st;
        return members(
            member("Name", &VertexAttributeInfo::Name),
            member("Location", &VertexAttributeInfo::Location),
            member("Binding", &VertexAttributeInfo::Binding),
            member("Offset", &VertexAttributeInfo::Offset),
            member("Type", &VertexAttributeInfo::GetTypeStr, &VertexAttributeInfo::SetTypeWithStr)
        );
    }

    template<>
    inline auto registerMembers<st::StageAttributes>() {
        using namespace st;
        return members(
            member("Stage", &StageAttributes::GetStageStr, &StageAttributes::SetStageWithStr),
            member("InputAttributes", &StageAttributes::InputAttributes),
            member("OutputAttributes", &StageAttributes::OutputAttributes)
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

    template<typename T0, typename T1>
    nlohmann::json serialize(const std::pair<T0, T1>& pair) {
        nlohmann::json result;
        result[0] = pair.first;
        result[1] = pair.second;
        return result;
    }

    template<typename Key, typename Value>
    nlohmann::json serialize(std::map<Key, Value>& _map) {
        nlohmann::json result;
        for (auto& entry : _map) {
            result.push_back(entry);
        }
        return result;
    }

    template<typename Key, typename Value>
    nlohmann::json deserialize(std::unordered_map<Key, Value>& _map) {
        nlohmann::json result;
        for (auto& entry : _map) {
            result.push_back(entry);
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