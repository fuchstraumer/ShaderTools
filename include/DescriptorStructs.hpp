#pragma once
#ifndef SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP
#define SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP
#include "CommonInclude.hpp"
#include <string>
#include <vector>
#include <map>

namespace st {

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
        VkFormat Format = VK_FORMAT_UNDEFINED;
        bool operator==(const DescriptorObject& other);
        bool operator<(const DescriptorObject& other);

        explicit operator VkDescriptorSetLayoutBinding() const;
        std::string GetType() const;
        void SetType(std::string type_str);

    };

    struct DescriptorSetInfo {
        uint32_t Index = std::numeric_limits<uint32_t>::max();
        std::map<uint32_t, DescriptorObject> Members = std::map<uint32_t, DescriptorObject>{};
    };

    struct PushConstantInfo {
        VkShaderStageFlags Stages;
        std::string Name;
        std::vector<ShaderDataObject> Members;
        uint32_t Offset;
        explicit operator VkPushConstantRange() const noexcept;
    };

}

#endif //!SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP