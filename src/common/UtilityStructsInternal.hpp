#pragma once
#ifndef SHADERTOOLS_UTILITY_STRUCTS_INTERNAL_HPP
#define SHADERTOOLS_UTILITY_STRUCTS_INTERNAL_HPP
#include "common/UtilityStructs.hpp"
#include "common/ShaderToolsErrors.hpp"

namespace st
{

    ShaderToolsErrorCode CountDescriptorType(const VkDescriptorType& type, descriptor_type_counts_t& typeCounts);

}

#endif //!SHADERTOOLS_UTILITY_STRUCTS_INTERNAL_HPP
