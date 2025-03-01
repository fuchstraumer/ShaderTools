#include "ShaderPackImpl.hpp"
#include "ShaderImpl.hpp"
#include "resources/ShaderResource.hpp"
#include "resources/ResourceGroup.hpp"
#include "../../common/impl/SessionImpl.hpp"
#include "../../generation/impl/ShaderStageProcessor.hpp"
#include "../../reflection/impl/ShaderReflectorImpl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include "../../common/UtilityStructsInternal.hpp"
#include <filesystem>
#include <iostream>
#include <format>

namespace st
{
    namespace fs = std::filesystem;

    ShaderPackImpl::ShaderPackImpl(const char* shader_pack_file_path, SessionImpl* error_session) :
        filePack(std::make_unique<yamlFile>(shader_pack_file_path, error_session)),
        workingDir(shader_pack_file_path),
        errorSession(error_session)
    {
        workingDir = fs::canonical(workingDir);
        packPath = workingDir.string();
        workingDir = workingDir.remove_filename();
        processShaderStages();

        createShaders();
        createResourceGroups();
        setDescriptorTypeCounts();
    }

    ShaderPackImpl::~ShaderPackImpl() {}

    void ShaderPackImpl::createPackScript(const char* fname)
    {
        filePack = std::make_unique<yamlFile>(fname, errorSession);
    }

    ShaderToolsErrorCode ShaderPackImpl::processShaderStages()
    {
        const std::string working_dir_str = workingDir.string();
        const std::vector<std::string> base_includes{ working_dir_str };

        std::vector<WriteRequest> write_requests;
        // Handle body paths first
        for (const auto& stage : filePack->stages)
        {
            fs::path body_path = fs::canonical(workingDir / fs::path(stage.first));

            if (fs::exists(body_path))
            {
                write_requests.emplace_back(WriteRequest::Type::AddShaderBodyPath, stage.second, std::move(body_path));
            }
            else
            {
                std::string errorStr = "Shader pack found shader with name " + stage.first + " had invalid path " + body_path.string();
				errorSession->AddError(
					this,
					ShaderToolsErrorSource::ShaderPack,
					ShaderToolsErrorCode::FilesystemPathDoesNotExist,
					errorStr.c_str());
                return ShaderToolsErrorCode::FilesystemPathDoesNotExist;
            }
        }

        ShaderToolsErrorCode batchWriteError = MakeFileTrackerBatchWriteRequest(std::move(write_requests));
        if (!WasWriteRequestSuccessful(batchWriteError))
        {
            errorSession->AddError(this, ShaderToolsErrorSource::ShaderPack, batchWriteError, "ShaderPack failed to write body paths to file tracker, exiting");
            return batchWriteError;
        }

        for (const auto& stage : filePack->stages)
		{
			fs::path body_path = fs::canonical(workingDir / fs::path(stage.first));
            processors.emplace(stage.second, std::make_unique<ShaderStageProcessor>(stage.second, filePack.get()));
            processorFutures.emplace(stage.second, 
				std::async(std::launch::async,
					&ShaderStageProcessor::Process,
					processors.at(stage.second).get(),
					stage.first,
					body_path.string(),
					filePack->stageExtensions[stage.second],
					base_includes));
        }

        return ShaderToolsErrorCode::Success;
    }

    void ShaderPackImpl::createShaders()
    {
        for (const auto& group : filePack->shaderGroups)
        {
            std::vector<ShaderStage> stages{};
            std::unique_copy(group.second.cbegin(), group.second.cend(), std::back_inserter(stages));
            createSingleGroup(group.first, stages);
        }
    }

    void ShaderPackImpl::createResourceGroups()
    {
        for (const auto& entry : filePack->resourceGroups)
        {
            resourceGroups.emplace(entry.first, std::make_unique<ResourceGroup>(filePack.get(), entry.first.c_str(), errorSession));
        }
    }

    void ShaderPackImpl::createSingleGroup(const std::string& name, const std::vector<ShaderStage> shaders)
    {
        for (const auto& shader : shaders)
        {
            // make sure processing is done
            try
            {
                if (processorFutures.count(shader))
                {
                    processorFutures.at(shader).get();
                    processorFutures.erase(shader);
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << e.what() << "\n";
                throw e;
            }
        }

        // Merge all error sessions together
        for (auto& processor : processors)
        {
            Session::MergeSessions(errorSession, std::move(processor.second->ErrorSession));
        }

        groups.emplace(name, std::make_unique<Shader>(name.c_str(), shaders.size(), shaders.data(), filePack.get(), errorSession));
    }

    void ShaderPackImpl::setDescriptorTypeCounts() const
    {
        const auto& sets = filePack->resourceGroups;
        for (const auto& resource_set : sets)
        {
            for (const auto& resource : resource_set.second)
            {
                ShaderToolsErrorCode error = CountDescriptorType(resource.DescriptorType(), typeCounts);
				if (error != ShaderToolsErrorCode::Success)
				{
					std::string errorStr = std::format("Shader pack failed to count descriptor type for resource {}", resource.Name());
					errorSession->AddError(this, ShaderToolsErrorSource::ShaderPack, error, errorStr.c_str());
				}
            }
        }
    }

}
