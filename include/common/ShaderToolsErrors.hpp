#pragma once
#ifndef SHADERTOOLS_ERRORS_HPP
#define SHADERTOOLS_ERRORS_HPP
#include "CommonInclude.hpp"
#include <system_error>

namespace st
{

    enum class ShaderToolsErrorCode : uint16_t
    {
        Success = 0,
        InvalidErrorCode = 1,
        // Initial input data file errors
        // For now, YAML file parser errors
        ParserErrorsStart = 2,
        ParserFileNotFound,
        ParserHadNoShaderGroups,
        ParserHadNoShadersInGroup,
        ParserHadInvalidOrEmptyTagsArray,
        ParserHadNoResourceGroups,
        ParserMissingResourceTypeSpecifier,
        ParserResourceTypeSpecifierNoVulkanEquivalent,
        GeneratorErrorsStart = 100,
        CompilerErrorsStart = 200,
        ReflectionErrorsStart = 300,
        // Errors not in the core pipeline of systems
        SubsystemErrorsStart = 400,
    };

    // Implemented in ShaderToolsErrors.cpp. Wanted all files to be able to access this,
    // without extra includes
    ST_API const std::error_category& ErrorCategory() noexcept;
    ST_API std::error_code MakeErrorCode(uint16_t stErrorValue) noexcept;
    ST_API const char* GetErrorCodeText(uint16_t stErrorValue) noexcept;

}


#endif //!SHADERTOOLS_ERRORS_HPP
