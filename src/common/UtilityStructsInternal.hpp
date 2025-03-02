#pragma once
#ifndef SHADERTOOLS_UTILITY_STRUCTS_INTERNAL_HPP
#define SHADERTOOLS_UTILITY_STRUCTS_INTERNAL_HPP
#include "common/UtilityStructs.hpp"
#include "common/ShaderToolsErrors.hpp"
#include <vector>
#include <optional>
#include <filesystem>

namespace st
{

    ShaderToolsErrorCode CountDescriptorType(const VkDescriptorType& type, descriptor_type_counts_t& typeCounts);

    struct ShaderBinaryData
    {
        std::vector<uint32_t> spirvForReflection;
        std::optional<std::vector<uint32_t>> optimizedSpirv;
    };

    
    struct ShaderCompilerOptions
    {
        enum class OptimizationLevel : uint8_t
        {
            Disabled = 0,
            Performance = 1,
            Size = 2
        };

        enum class TargetVersionEnum : uint8_t
        {
            Vulkan1_0 = 0,
            Vulkan1_1 = 1,
            Vulkan1_2 = 2,
            Vulkan1_3 = 3,
            Vulkan1_4 = 4,
            VulkanLatest = 4,
            OpenGL4_5 = 5,
        };

        enum class SourceLanguage : uint8_t
        {
            GLSL = 0,
            HLSL = 1,
            Metal = 2,  
        };

        OptimizationLevel Optimization = OptimizationLevel::Performance;
        TargetVersionEnum TargetVersion = TargetVersionEnum::VulkanLatest;
        SourceLanguage SourceLanguage = SourceLanguage::GLSL;
        bool GenerateDebugInfo = false;
        std::vector<std::filesystem::path> IncludePaths;
    };

}

#endif //!SHADERTOOLS_UTILITY_STRUCTS_INTERNAL_HPP
