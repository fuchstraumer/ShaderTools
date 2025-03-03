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
        ParserYamlFileHadInvalidOptimizationLevel,
        ParserResourceFormatNoVulkanEquivalent,
        ParserYamlFileHadInvalidTargetVersion,

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
        
    };

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
    };

    // We don't include a file path, because that will be part of the error message we store
    // in the error structure anyways.
    struct SourceLocation
    {
        size_t line;
        size_t column;
        const char* filename;
        const char* function_name;
    };


    ST_API const char* ErrorCodeToText(ShaderToolsErrorCode code) noexcept;
    ST_API const char* ErrorSourceToText(ShaderToolsErrorSource source) noexcept;

    ST_API ShaderToolsErrorCode ClearAllInternalStorage();
}

namespace std
{
    template<>
    struct is_error_code_enum<st::ShaderToolsErrorCode> : true_type {};
} // namespace std


#endif //!SHADERTOOLS_ERRORS_HPP
