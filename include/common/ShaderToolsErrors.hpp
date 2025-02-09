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
        // Nearly all of these are results of input data, so they are part of the
        // input data error category. Some of these may be fixable by sanitization,
        // but most of these should be fixed by the end-user inputting data
        ParserErrorsStart = 2,
        ParserFileNotFound,
        ParserHadNoShaderGroups,
        ParserHadNoShadersInGroup,
        ParserHadInvalidOrEmptyTagsArray,
        ParserHadNoResourceGroups,
        ParserMissingResourceTypeSpecifier,
        ParserResourceTypeSpecifierNoVulkanEquivalent,
        ParserYamlFileHadNoShadersInGroup,
        ParserYamlFileHadInvalidOrEmptyTagsArray,
        GeneratorErrorsStart = 100,
        // In the case of multiple errors, we return this to the top level. Asking the generator for the full
        // errors can give the array of errors for printing or logging elsewhere.
        GeneratorMultipleErrorsFound,
        GeneratorEmptyIncludePathArray,
        GeneratorInvalidDescriptorTypeInResourceBlock,
        GeneratorShaderBodyStringNotFound,
        GeneratorUnableToFindPreambleFile,
        GeneratorFoundEmptyBodyString,
        GeneratorInvalidResourceQualifier,
        // Errors here mostly come from issues compiling, or invalid use of otherwise valid shader code.
        // This is the ShaderToolsInternal error category, the stuff that's most on this library to fix
        CompilerErrorsStart = 200,
        ReflectionErrorsStart = 300,
        // Errors not in the core pipeline of systems. Error category - Filesystem
        SubsystemErrorsStart = 400,
        FilesystemPathDoesNotExist,
        FilesystemPathExistedFileCouldNotBeOpened,
        FilesystemCouldNotEmplaceIntoInternalStorage,
        FilesystemNoFileDataForGivenHandleFound
    };

    // Implemented in ShaderToolsErrors.cpp. Wanted all files to be able to access this,
    // without extra includes
    ST_API const std::error_category& ErrorCategory() noexcept;
    ST_API std::error_code MakeErrorCode(uint16_t stErrorValue) noexcept;
    ST_API const char* GetErrorCodeText(uint16_t stErrorValue) noexcept;

}

namespace std
{
    template<>
    struct is_error_code_enum<st::ShaderToolsErrorCode> : true_type {};
} // namespace std


#endif //!SHADERTOOLS_ERRORS_HPP
