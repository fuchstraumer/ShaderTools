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

    /**
     * @brief Stores both optimized and unoptimized SPIR-V binaries for a single shader stage, as we need both
     * 
     * spirvForReflection is not optional, as it is always required for the reflection system to use and introspect on. The 
     * optimizedSpirv is optional, as it may not be available if the shader failed the optimized compile (which can occur spuriously),
     * or if the user does not want to optimize this particular shader.
     */
    struct ShaderBinaryData
    {
        std::vector<uint32_t> spirvForReflection;
        std::optional<std::vector<uint32_t>> optimizedSpirv;
    };


    /**
     * @brief Configuration options for the shader compiler
     */
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
    
    /**
     * @brief Parser representation of a structured type. Only contains key attributes and members list.
     * @note This is an intermediate representation, only used as output from parsing and is not yet valid to use
     * @see GeneratedStructureSchema for the final representation that can be substituted into shader source code
     */
    using ParsedStructureSchema = std::vector<std::string>;

    /**
     * @brief Generated structure schema, which can be plugged into shader source code. Prefixes and formatting changes slightly based on bindless/not bindless.
     * @see ParsedStructureSchema for the intermediate representation used during parsing
     */
    struct GeneratedStructureSchema
    {
        std::vector<std::string_view> BufferMembers;
        std::string GeneratedString;
        size_t Size{ 0u };
        size_t Alignment{ 0u };
        
    };

}

#endif //!SHADERTOOLS_UTILITY_STRUCTS_INTERNAL_HPP
