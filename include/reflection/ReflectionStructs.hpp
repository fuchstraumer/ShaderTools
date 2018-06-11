#pragma once
#ifndef SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP
#define SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"
#include "spirv-cross/spirv_cross.hpp"
#include <string>
#include <vector>
#include <map>

namespace st {

    struct PushConstantInfoImpl;
    struct VertexAttributeInfoImpl;
    struct StageAttributesImpl;
  
    struct PushConstantInfo {
        VkShaderStageFlags Stages;
        std::string Name;
        std::vector<ShaderResourceSubObject> Members;
        uint32_t Offset;
        explicit operator VkPushConstantRange() const noexcept;
    };

    struct VertexAttributeInfo {
        std::string Name;
        spirv_cross::SPIRType Type;
        std::string GetTypeStr() const;
        void SetTypeWithStr(std::string str);
        explicit operator VkVertexInputAttributeDescription() const;
        uint32_t Location;
        uint32_t Offset;
    };

    struct StageAttributes {
        std::map<uint32_t, VertexAttributeInfo> InputAttributes;
        std::map<uint32_t, VertexAttributeInfo> OutputAttributes;
        VkShaderStageFlags Stage;
        void SetStageWithStr(std::string str);
        std::string GetStageStr() const;
    };

}

#endif //!SHADER_TOOLS_DESCRIPTOR_STRUCTS_HPP
