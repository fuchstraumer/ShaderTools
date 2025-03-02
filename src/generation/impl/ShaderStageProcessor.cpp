#include "ShaderStageProcessor.hpp"
#include "../../common/impl/SessionImpl.hpp"
#include "../../generation/impl/ShaderGeneratorImpl.hpp"
#include "../../generation/impl/CompilerImpl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include "../../parser/yamlFile.hpp"
#include <filesystem>

namespace st
{

    namespace fs = std::filesystem;

    ShaderStageProcessor::ShaderStageProcessor(ShaderStage _stage, yamlFile* rfile) :
        ErrorSession{},
        stage(std::move(_stage)),
        rsrcFile(rfile),
        bodyPath{},
        generator(std::make_unique<ShaderGeneratorImpl>(_stage, ErrorSession.GetImpl())),
        compiler(std::make_unique<ShaderCompilerImpl>(rfile->compilerOptions, ErrorSession.GetImpl()))
    {
        generator->resourceFile = rsrcFile;
    }

    ShaderStageProcessor::~ShaderStageProcessor() {}

    void ShaderStageProcessor::Process(
        std::string shader_name,
        std::string body_path,
        std::vector<std::string> extensions,
        std::vector<std::string> includes)
    {
        bodyPath = std::move(body_path);
        std::string full_source_string = Generate(bodyPath, extensions, includes);
        if (!full_source_string.empty())
        {
			std::vector<uint32_t> compiledBinary = Compile(shader_name, std::move(full_source_string));
        }
    }

    std::string ShaderStageProcessor::Generate(const std::string& body_path_str, const std::vector<std::string>& extensions, const std::vector<std::string>& includes)
    {
        static const std::string emptyStringResult{};

        WriteRequest addBodyPathRequest{ WriteRequest::Type::AddShaderBodyPath, stage, std::filesystem::path(body_path_str) };
        ShaderToolsErrorCode writeResult = MakeFileTrackerWriteRequest(std::move(addBodyPathRequest));
        if (!WasWriteRequestSuccessful(writeResult))
        {
            ErrorSession.GetImpl()->AddError(this, ShaderToolsErrorSource::ShaderStageProcessor, writeResult, "ShaderStageProcessor couldn't add path to body string, generation failed");
            return emptyStringResult;
        }

        generator->includes.insert(generator->includes.end(), includes.begin(), includes.end());

        for (const auto& ext : extensions)
        {
            generator->addExtension(ext);
        }

        // Decide if we're gonna generate
        ReadRequest generationDecisionReadReq{ ReadRequest::Type::HasFullSourceString, stage };
        ReadRequestResult generationDecision = MakeFileTrackerReadRequest(generationDecisionReadReq);

        [[unlikely]]
        if (!generationDecision.has_value())
        {
            ErrorSession.GetImpl()->AddError(this, ShaderToolsErrorSource::ShaderStageProcessor, generationDecision.error(), "ShaderStageProcessor couldn't query if full source string existed already, aborting");
            return emptyStringResult;
        }
        else if (std::get<bool>(*generationDecision))
        {
            // Shader full source string already exists, now we need to see if there's been source changes since we last generated it.
            fs::path actual_path{ body_path_str };

            if (!fs::exists(actual_path))
            {
                ErrorSession.GetImpl()->AddError(this, ShaderToolsErrorSource::ShaderStageProcessor, ShaderToolsErrorCode::ShaderStageProcessorGivenBodyPathStringDidNotExist, body_path_str.c_str());
                return emptyStringResult;
            }

            actual_path = fs::canonical(actual_path);

            // Might have potentially generated already. Lets check timestamps.
            ReadRequest lastModTimeRequest{ ReadRequest::Type::FindLastModificationTime, stage };
            ReadRequestResult readResult = MakeFileTrackerReadRequest(lastModTimeRequest);
            if (!readResult.has_value())
            {
                const std::string actualPathStr = actual_path.string();
                const std::string errorMessage = "Shader stage processor could not retrieve last write time of body path: " + actualPathStr;
                ErrorSession.GetImpl()->AddError(this, ShaderToolsErrorSource::ShaderStageProcessor, readResult.error(), errorMessage.c_str());
                return emptyStringResult;
            }

			auto curr_write_time = fs::last_write_time(actual_path);
            auto stored_write_time = std::get<std::filesystem::file_time_type>(*readResult);

            if (curr_write_time > stored_write_time)
            {
                std::vector<WriteRequest> writeRequests
                {
                    WriteRequest{ WriteRequest::Type::AddShaderBodyPath, stage, actual_path },
                    WriteRequest{ WriteRequest::Type::UpdateModificationTime, stage, curr_write_time }
                };

                ShaderToolsErrorCode writeError = MakeFileTrackerBatchWriteRequest(std::move(writeRequests));
                if (!WasWriteRequestSuccessful(writeError))
                {
                    ErrorSession.GetImpl()->AddError(this, ShaderToolsErrorSource::ShaderStageProcessor, writeError, "Failed to update shader body path and last modification time, following source changes since last generation");
                }

                std::vector<EraseRequest> eraseRequests
                {
                    EraseRequest{ EraseRequest::Type::Required, EraseRequest::Target::ShaderBody, stage },
                    EraseRequest{ EraseRequest::Type::Required, EraseRequest::Target::FullSourceString, stage },
                    EraseRequest{ EraseRequest::Type::Optional, EraseRequest::Target::BinarySource, stage }
                };

                ShaderToolsErrorCode eraseError = MakeFileTrackerBatchEraseRequest(std::move(eraseRequests));
                if (eraseError != ShaderToolsErrorCode::Success)
                {
                    ErrorSession.GetImpl()->AddError(this, ShaderToolsErrorSource::ShaderStageProcessor, eraseError, "Failed to erase existing generated source, unable to propagate source changes since last generation");
                }

                if ((writeError != ShaderToolsErrorCode::Success) || (eraseError != ShaderToolsErrorCode::Success))
                {
                    return emptyStringResult;
                }

            }
            else
            {
                ReadRequest fullSourceReadReq{ ReadRequest::Type::FindFullSourceString, stage };
                ReadRequestResult fullSourceResult = MakeFileTrackerReadRequest(fullSourceReadReq);
                if (fullSourceResult.has_value())
                {
                    return std::get<std::string>(*fullSourceResult);
                }
                else
                {
                    // I have no idea how we would even get here
                    std::string actualPathStr = actual_path.string();
                    const std::string errorString = "Couldn't retrieve existing full source string for shader at following path, despite it existing in storage: " + actualPathStr;
                    ErrorSession.GetImpl()->AddError(this, ShaderToolsErrorSource::ShaderStageProcessor, fullSourceResult.error(), errorString.c_str());
                    return emptyStringResult;
                }
            }
        }
        else
        {
            ShaderToolsErrorCode generationError = generator->generate(stage, body_path_str, 0u, nullptr);
            if (generationError != ShaderToolsErrorCode::Success)
            {
                ErrorSession.GetImpl()->AddError(this, ShaderToolsErrorSource::ShaderStageProcessor, generationError, nullptr);
                return emptyStringResult;
            }
            else
            {
                return generator->getFullSource();
            }
        }

        return emptyStringResult;
    }

    std::vector<uint32_t> ShaderStageProcessor::Compile(std::string shader_name, std::string full_source_string)
    {
        ReadRequest binaryReadReq{ ReadRequest::Type::FindShaderBinary, stage };
        ReadRequestResult binaryReadResult = MakeFileTrackerReadRequest(binaryReadReq);
        if (binaryReadResult.has_value())
        {
            return std::get<std::vector<uint32_t>>(*binaryReadResult);
        }
        else
        {
			ShaderToolsErrorCode compileError = compiler->prepareToCompile(stage, std::move(shader_name), std::move(full_source_string));
			if (compileError == ShaderToolsErrorCode::Success)
			{
				binaryReadResult = MakeFileTrackerReadRequest(binaryReadReq);

				[[likely]]
				if (binaryReadResult.has_value())
				{
					return std::get<std::vector<uint32_t>>(*binaryReadResult);
				}
				else
				{
					ErrorSession.GetImpl()->AddError(
						this,
						ShaderToolsErrorSource::ShaderStageProcessor,
						binaryReadResult.error(),
						"Could not read compiled binary string from storage after successful compilation");
					return std::vector<uint32_t>{};
				}
			}
			else
			{
				ErrorSession.GetImpl()->AddError(
					this,
					ShaderToolsErrorSource::ShaderStageProcessor,
					compileError,
					"ShaderStageProcessor's attempt at compiling shader was not successful");
				return std::vector<uint32_t>{};
			}
        }
    }

}
