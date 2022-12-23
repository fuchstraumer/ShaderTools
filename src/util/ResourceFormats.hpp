#pragma once
#ifndef ST_RESOURCE_FORMATS_HPP
#define ST_RESOURCE_FORMATS_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"
#include "spirv-cross/spirv_cross.hpp"
#include <string>

namespace st
{

    std::string GetDescriptorTypeString(const VkDescriptorType& type);
    VkDescriptorType GetDescriptorTypeFromString(const std::string& str);

    std::string GetShaderStageString(const VkShaderStageFlags& flag);
    VkShaderStageFlags GetShaderStageFromString(const std::string& str);

    std::string SPIR_TypeToString(const spirv_cross::SPIRType& stype);
    spirv_cross::SPIRType SPIR_TypeFromString(const std::string& str);
    spirv_cross::SPIRType::BaseType SPIR_BaseTypeEnumFromString(const std::string& str);

    // Image formats (mainly)
    VkFormat VkFormatFromSPIRType(const spirv_cross::SPIRType& type);
    VkFormat VkFormatFromString(const std::string& string);
    std::string VkFormatEnumToString(const VkFormat& string);
    size_t VkFormatSize(const VkFormat& fmt);

}

#endif //!ST_RESOURCE_FORMATS_HPP
