#pragma once
#ifndef SHADERTOOLS_ERRORS_HPP
#define SHADERTOOLS_ERRORS_HPP
#include "CommonInclude.hpp"
#include <system_error>

namespace st
{
    /**
     * @brief Contains verbose and specific errors for most internal errors that can occur in ShaderTools
     * 
     * Error codes are grouped into categories, such as Parser, Generator, Compiler, Reflection, Filesystem, etc. Each category
     * is prefixed with the name of that category, followed by specific error information. String conversion is provided
     * for each error code, so that users can easily convert error codes to human-readable strings
     * @ingroup Common
     */
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
        ParserYamlFileHadInvalidOptimizationLevel,
        ParserResourceFormatNoVulkanEquivalent,
        ParserYamlFileHadInvalidTargetVersion,
        ParserYamlFileHadNoBufferReferences,
        ParserYamlFileHadInvalidBufferReferenceAlignment,
        ParserYamlFileHadNoRequiredExtensions,
        ParserRequiredExtensionNotAString,

        GeneratorErrorsStart = 100,
        GeneratorEmptyIncludePathArray,
        GeneratorInvalidDescriptorTypeInResourceBlock,
        GeneratorShaderBodyStringNotFound,
        GeneratorUnableToFindPreambleFile,
        GeneratorUnableToStoreFileContents,
        GeneratorUnableToAddPreambleToInstanceStorage,
        GeneratorUnableToFindMatchingElementOfVertexInterfaceBlockNeededForCompletion,
        GeneratorFoundEmptyBodyString,
        GeneratorUnableToStoreFragmentFileContents,
        GeneratorFragmentFileNotFound,
        GeneratorInvalidResourceQualifier,
        GeneratorUnableToAddInterface,
        GeneratorUnableToParseInterfaceBlock,
        GeneratorUnableToStoreFullSourceString,
        GeneratorUnableToFindLibraryInclude,
        GeneratorUnableToFindLocalInclude,
        GeneratorInvalidImageType,
        GeneratorUnableToAddShaderBodyPath,
        GeneratorUnableToFindEndingOfInterfaceOverride,
        GeneratorNoBodyStringInFileTrackerStorage,

        CompilerErrorsStart = 200,
        CompilerShaderKindNotSupported,
        CompilerShaderCompilationFailed,
        
        ReflectionErrorsStart = 300,
        ReflectionInvalidDescriptorType,
        ReflectionInvalidResource,
        ReflectionInvalidBindingIndex,
        ReflectionShaderBinaryNotFound, 
        ReflectionInvalidSpecializationConstantType,
        ReflectionRecompilerError,
        ReflectionCouldNotStoreResource,
        ReflectionMultiplePushConstants,
        ReflectionFailedToParseInputAttributes,
        ReflectionFailedToParseOutputAttributes,

        SpvReflectErrorsStart = 350,


        ResourceErrorsStart = 400,
        ResourceInvalidDescriptorType,
        ResourceNotFound,

        ShaderPackErrorsStart = 500,
        ShaderPackInvalidDescriptorType,

        ShaderErrorsStart = 550,
        ShaderDoesNotContainGivenHandle,

        SubsystemErrorsStart = 600,
        FilesystemPathDoesNotExist,
        // Message is the path that was attempted
        FilesystemPathExistedFileCouldNotBeOpened,
        FilesystemCouldNotEmplaceIntoInternalStorage,
        FilesystemNoFileDataForGivenHandleFound,
        FilesystemFailedToReadValidFileStream,

        FileTrackerErrorsStart = 700,
        FileTrackerInvalidRequest,
        FileTrackerReadRequestFailed,
        FileTrackerBatchReadRequestFailed,
        FileTrackerWriteRequestFailed,
        FileTrackerBatchWriteRequestFailed,
        FileTrackerEraseRequestFailed,
        FileTrackerBatchEraseRequestFailed,
        FileTrackerWriteCouldNotAddPayloadToStorage,
        FileTrackerPayloadAlreadyStored,

        ShaderStageProcessorErrorsStart = 800,
        ShaderStageProcessorGivenBodyPathStringDidNotExist,

        IncludeHandlerErrorsStart = 900,
        IncludeHandlerFileNotFound,

        SchemaParserErrorsStart = 1000,

        AlignmentParserErrorsStart = 1100,
        AlignmentParserMissingEndBracketInArrayDeclaration,
        AlignmentParserInvalidArrayDimension,
        AlignmentParserUnknownGLSLType,
        
    };

    /**
     * @brief Represents the source or "domain" of the error, allowing for more specific categorization of errors
     * @ingroup Common
     */
    enum class ShaderToolsErrorSource : uint16_t
    {
        Parser,
        Generator,
        Compiler,
        Reflection,
        Filesystem,
        UserInput,
        ResourceGroup,
        ShaderPack,
        ShaderStageProcessor,
        IncludeHandler,
        SchemaParser,
        SchemaAlignmentParser,
    };

    /**
     * @brief Effectively a re-implementation of std::source_location, but to avoid including that header across the DLL boundary
     * @ingroup Common
     */
    struct SourceLocation
    {
        size_t line;
        size_t column;
        const char* filename;
        const char* function_name;
    };


    ST_API const char* ErrorCodeToText(ShaderToolsErrorCode code) noexcept;
    ST_API const char* ErrorSourceToText(ShaderToolsErrorSource source) noexcept;

    /** 
     * @brief Clears all internal storage used by the current instance (usually, DLL) of this library.
     * @see ShaderFileTracker for the storage that is actually cleared
     * @ingroup Common
     */
    ST_API ShaderToolsErrorCode ClearAllInternalStorage();
}

namespace std
{
    template<>
    struct is_error_code_enum<st::ShaderToolsErrorCode> : true_type {};
} // namespace std


#endif //!SHADERTOOLS_ERRORS_HPP
