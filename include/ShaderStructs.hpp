#pragma once
#ifndef SHADER_TOOLS_STRUCTS_HPP
#define SHADER_TOOLS_STRUCTS_HPP
#include "CommonInclude.hpp"
#include "DescriptorStructs.hpp"
#include "spirv-cross/spirv_cross.hpp"
#include <vector>
#include <map>

namespace st {

    std::string GetTypeString(const VkDescriptorType& type);
    VkDescriptorType GetTypeFromString(const std::string& str);

    std::string StageFlagToStr(const VkShaderStageFlags& flag);
    VkShaderStageFlags StrToStageFlags(const std::string& str);
        
    std::string TypeToStr(const spirv_cross::SPIRType& stype);
    spirv_cross::SPIRType::BaseType BaseTypeEnumFromStr(const std::string& str);
    spirv_cross::SPIRType TypeFromStr(const std::string& str);
    VkFormat FormatFromSPIRType(const spirv_cross::SPIRType& type);
    DescriptorObject::storage_class StorageClassFromSPIRType(const spirv_cross::SPIRType & type);

    struct VertexAttributeInfo {
        std::string Name;
        spirv_cross::SPIRType Type;
        std::string GetTypeStr() const;
        void SetTypeWithStr(std::string str);
        explicit operator VkVertexInputAttributeDescription() const noexcept;
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


#endif //!SHADER_TOOLS_STRUCTS_HPP