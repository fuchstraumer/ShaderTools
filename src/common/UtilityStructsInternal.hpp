#pragma once
#ifndef SHADERTOOLS_UTILITY_STRUCTS_INTERNAL_HPP
#define SHADERTOOLS_UTILITY_STRUCTS_INTERNAL_HPP
#include "common/UtilityStructs.hpp"
#include "common/ShaderToolsErrors.hpp"
#include <vector>
#include <optional>

namespace st
{

    ShaderToolsErrorCode CountDescriptorType(const VkDescriptorType& type, descriptor_type_counts_t& typeCounts);

    struct ShaderBinaryData
    {
        std::vector<uint32_t> spirvForReflection;
        std::optional<std::vector<uint32_t>> optimizedSpirv;
    };

}

#endif //!SHADERTOOLS_UTILITY_STRUCTS_INTERNAL_HPP
