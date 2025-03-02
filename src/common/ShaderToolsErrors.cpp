#include "common/ShaderToolsErrors.hpp"

namespace st
{
    namespace detail
    {
        // An issue with the actual content of what we're processing, thus category encompassses all issues
        // with input data
        class InputDataCategory : public std::error_category
        {
        public:
            const char* name() const noexcept override
            {
                return "ShaderToolsInputData";
            }

            std::string message(int ev) const override;
        };

        class InternalErrorCategory : public std::error_category
        {
        public:
            const char* name() const noexcept override
            {
                return "ShaderToolsInternal";
            }

            std::string message(int ev) const override;
        };

        // A ton of our failures are due to filesystem issues, either invalid paths or files we can't open
        class FilesystemCategory : public std::error_category
        {
        public:
            const char* name() const noexcept override
            {
                return "ShaderToolsFilesystem";
            }

            std::string message(int ev) const override;
        };

    } // namespace detail

    const char* ErrorCodeToText(ShaderToolsErrorCode code) noexcept
    {
        switch (code)
        {
            case ShaderToolsErrorCode::Success:
                return "Success";
            case ShaderToolsErrorCode::InvalidErrorCode:
                return "InvalidErrorCode";
            case ShaderToolsErrorCode::ParserErrorsStart:
                return "ParserErrorsStart";
            case ShaderToolsErrorCode::ParserFileNotFound:
                return "ParserFileNotFound";
            case ShaderToolsErrorCode::ParserHadNoShaderGroups:
                return "ParserHadNoShaderGroups";
            case ShaderToolsErrorCode::ParserHadNoShadersInGroup:
                return "ParserHadNoShadersInGroup";
            case ShaderToolsErrorCode::ParserHadInvalidOrEmptyTagsArray:
                return "ParserHadInvalidOrEmptyTagsArray";
            case ShaderToolsErrorCode::ParserHadNoResourceGroups:
                return "ParserHadNoResourceGroups";
            case ShaderToolsErrorCode::ParserMissingResourceTypeSpecifier:
                return "ParserMissingResourceTypeSpecifier";
            case ShaderToolsErrorCode::ParserResourceTypeSpecifierNoVulkanEquivalent:
                return "ParserResourceTypeSpecifierNoVulkanEquivalent";
            case ShaderToolsErrorCode::ParserYamlFileHadNoShadersInGroup:
                return "ParserYamlFileHadNoShadersInGroup";
            case ShaderToolsErrorCode::ParserYamlFileHadInvalidOrEmptyTagsArray:
                return "ParserYamlFileHadInvalidOrEmptyTagsArray";
            case ShaderToolsErrorCode::ParserYamlFileHadInvalidOptimizationLevel:
                return "ParserYamlFileHadInvalidOptimizationLevel";
            case ShaderToolsErrorCode::ParserResourceFormatNoVulkanEquivalent:
                return "ParserResourceFormatNoVulkanEquivalent";
            case ShaderToolsErrorCode::ParserYamlFileHadInvalidTargetVersion:
                return "ParserYamlFileHadInvalidTargetVersion";
            case ShaderToolsErrorCode::GeneratorEmptyIncludePathArray:
                return "GeneratorEmptyIncludePathArray";
            case ShaderToolsErrorCode::GeneratorInvalidDescriptorTypeInResourceBlock:
                return "GeneratorInvalidDescriptorTypeInResourceBlock";
            case ShaderToolsErrorCode::GeneratorShaderBodyStringNotFound:
                return "GeneratorShaderBodyStringNotFound";
            case ShaderToolsErrorCode::GeneratorUnableToFindPreambleFile:
                return "GeneratorUnableToFindPreambleFile";
            case ShaderToolsErrorCode::GeneratorUnableToStoreFileContents:
                return "GeneratorUnableToStoreFileContents";
            case ShaderToolsErrorCode::GeneratorUnableToAddPreambleToInstanceStorage:
                return "GeneratorUnableToAddPreambleToInstanceStorage";
            case ShaderToolsErrorCode::GeneratorUnableToFindMatchingElementOfVertexInterfaceBlockNeededForCompletion:
                return "GeneratorUnableToFindMatchingElementOfVertexInterfaceBlockNeededForCompletion";
            case ShaderToolsErrorCode::GeneratorFoundEmptyBodyString:
                return "GeneratorFoundEmptyBodyString";
            case ShaderToolsErrorCode::GeneratorUnableToStoreFragmentFileContents:
                return "GeneratorUnableToStoreFragmentFileContents";
            case ShaderToolsErrorCode::GeneratorFragmentFileNotFound:
                return "GeneratorFragmentFileNotFound";
            case ShaderToolsErrorCode::GeneratorInvalidResourceQualifier:
                return "GeneratorInvalidResourceQualifier";
            case ShaderToolsErrorCode::GeneratorUnableToAddInterface:
                return "GeneratorUnableToAddInterface";
            case ShaderToolsErrorCode::GeneratorUnableToParseInterfaceBlock:
                return "GeneratorUnableToParseInterfaceBlock";
            case ShaderToolsErrorCode::GeneratorUnableToStoreFullSourceString:
                return "GeneratorUnableToStoreFullSourceString";
            case ShaderToolsErrorCode::GeneratorUnableToFindLibraryInclude:
                return "GeneratorUnableToFindLibraryInclude";
            case ShaderToolsErrorCode::GeneratorUnableToFindLocalInclude:
                return "GeneratorUnableToFindLocalInclude";
            case ShaderToolsErrorCode::GeneratorInvalidImageType:
                return "GeneratorInvalidImageType";
            case ShaderToolsErrorCode::GeneratorUnableToAddShaderBodyPath:
                return "GeneratorUnableToAddShaderBodyPath";
            case ShaderToolsErrorCode::GeneratorUnableToFindEndingOfInterfaceOverride:
                return "GeneratorUnableToFindEndingOfInterfaceOverride";
            case ShaderToolsErrorCode::GeneratorNoBodyStringInFileTrackerStorage:
                return "GeneratorNoBodyStringInFileTrackerStorage";
            case ShaderToolsErrorCode::CompilerShaderKindNotSupported:
                return "CompilerShaderKindNotSupported";
            case ShaderToolsErrorCode::CompilerShaderCompilationFailed:
                return "CompilerShaderCompilationFailed";
            case ShaderToolsErrorCode::ReflectionInvalidDescriptorType:
                return "ReflectionInvalidDescriptorType";
            case ShaderToolsErrorCode::ReflectionInvalidResource:
                return "ReflectionInvalidResource";
            case ShaderToolsErrorCode::ReflectionInvalidBindingIndex:
                return "ReflectionInvalidBindingIndex";
            case ShaderToolsErrorCode::ReflectionShaderBinaryNotFound:
                return "ReflectionShaderBinaryNotFound";
            case ShaderToolsErrorCode::ReflectionInvalidSpecializationConstantType:
                return "ReflectionInvalidSpecializationConstantType";
            case ShaderToolsErrorCode::ReflectionRecompilerError:
                return "ReflectionRecompilerError";
            case ShaderToolsErrorCode::ResourceInvalidDescriptorType:
                return "ResourceInvalidDescriptorType";
            case ShaderToolsErrorCode::ResourceNotFound:
                return "ResourceNotFound";
            case ShaderToolsErrorCode::ShaderPackInvalidDescriptorType:
                return "ShaderPackInvalidDescriptorType";
            case ShaderToolsErrorCode::ShaderDoesNotContainGivenHandle:
                return "ShaderDoesNotContainGivenHandle";
            case ShaderToolsErrorCode::SubsystemErrorsStart:
                return "SubsystemErrorsStart";
            case ShaderToolsErrorCode::FilesystemPathDoesNotExist:
                return "FilesystemPathDoesNotExist";
            case ShaderToolsErrorCode::FilesystemPathExistedFileCouldNotBeOpened:
                return "FilesystemPathExistedFileCouldNotBeOpened";
            case ShaderToolsErrorCode::FilesystemCouldNotEmplaceIntoInternalStorage:
                return "FilesystemCouldNotEmplaceIntoInternalStorage";
            case ShaderToolsErrorCode::FilesystemNoFileDataForGivenHandleFound:
                return "FilesystemNoFileDataForGivenHandleFound";
            case ShaderToolsErrorCode::FileTrackerErrorsStart:
                return "FileTrackerErrorsStart";
            case ShaderToolsErrorCode::FileTrackerInvalidRequest:
                return "FileTrackerInvalidRequest";
            case ShaderToolsErrorCode::FileTrackerReadRequestFailed:
                return "FileTrackerReadRequestFailed";
            case ShaderToolsErrorCode::FileTrackerBatchReadRequestFailed:
                return "FileTrackerBatchReadRequestFailed";
            case ShaderToolsErrorCode::FileTrackerBatchWriteRequestFailed:
                return "FileTrackerBatchWriteRequestFailed";
            case ShaderToolsErrorCode::FileTrackerBatchEraseRequestFailed:
                return "FileTrackerBatchEraseRequestFailed";
            case ShaderToolsErrorCode::FileTrackerWriteCouldNotAddPayloadToStorage:
                return "FileTrackerWriteCouldNotAddPayloadToStorage";
            case ShaderToolsErrorCode::FileTrackerPayloadAlreadyStored:
                return "FileTrackerPayloadAlreadyStored";
            case ShaderToolsErrorCode::ShaderStageProcessorErrorsStart:
                return "ShaderStageProcessorErrorsStart";
            case ShaderToolsErrorCode::ShaderStageProcessorGivenBodyPathStringDidNotExist:
                return "ShaderStageProcessorGivenBodyPathStringDidNotExist";
            default:
                return "InvalidErrorCode: cannot find error string for this error code";
        }
    }

    const char* ErrorSourceToText(ShaderToolsErrorSource source) noexcept
    {
        switch (source)
        {
            case ShaderToolsErrorSource::Parser:
                return "Parser";
            case ShaderToolsErrorSource::Generator:
                return "Generator";
            case ShaderToolsErrorSource::Compiler:
                return "Compiler";
            case ShaderToolsErrorSource::Reflection:
                return "Reflection";
            case ShaderToolsErrorSource::Filesystem:
                return "Filesystem";
            case ShaderToolsErrorSource::UserInput:
                return "UserInput";
            case ShaderToolsErrorSource::ResourceGroup:
                return "ResourceGroup";
            case ShaderToolsErrorSource::ShaderPack:
                return "ShaderPack";
            case ShaderToolsErrorSource::ShaderStageProcessor:
                return "ShaderStageProcessor";
            default:
                return "InvalidErrorSource: cannot find error source string for this ErrorSource code";
        }
    }

}
