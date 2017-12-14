#pragma once
#ifndef SHADER_TOOLS_STRUCTS_HPP
#define SHADER_TOOLS_STRUCTS_HPP
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <cstdint>
#include <map>
namespace st {

    struct DescriptorObject {
        std::string Name;
        uint32_t Binding, ParentSet;
        VkShaderStageFlags Stages;
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
    };

    struct DescriptorSetInfo {
        uint32_t Index;
        std::multimap<VkDescriptorType, DescriptorObject> Members;
    };

    struct ShaderDataObject {
        std::string Name;
        std::string Type;
        uint32_t Size, Offset;
    };

    struct PushConstantInfo {
        VkShaderStageFlags Stages;
        std::vector<ShaderDataObject> Members;
    };

}

#endif //!SHADER_TOOLS_STRUCTS_HPP