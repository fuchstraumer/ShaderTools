#pragma once
#ifndef ST_RESOURCE_FORMATS_HPP
#define ST_RESOURCE_FORMATS_HPP
#include "common/CommonInclude.hpp"
#include "common/UtilityStructs.hpp"
#include <string>
#include <spirv_reflect.h>

namespace st
{

    std::string GetDescriptorTypeString(const VkDescriptorType& type);
    VkDescriptorType GetDescriptorTypeFromString(const std::string& str);

    std::string GetShaderStageString(const VkShaderStageFlags& flag);
    VkShaderStageFlags GetShaderStageFromString(const std::string& str);

    const char* spvReflect_TypeToString(const SpvReflectTypeFlags type);
    SpvReflectTypeFlags SPIR_TypeFromString(const std::string& str);

    // Image formats (mainly)
    VkFormat VkFormatFromSpvReflectFormat(const SpvReflectFormat type);
    VkFormat VkFormatFromString(const std::string& string);
    std::string VkFormatEnumToString(const VkFormat& string);
    size_t VkFormatSize(const VkFormat& fmt);

}

#endif //!ST_RESOURCE_FORMATS_HPP
