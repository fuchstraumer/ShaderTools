#include "ShaderPackImpl.hpp"
#include "ShaderImpl.hpp"
#include "resources/ShaderResource.hpp"
#include "resources/ResourceGroup.hpp"
#include "common/stSession.hpp"
#include "../../generation/impl/ShaderStageProcessor.hpp"
#include "../../reflection/impl/ShaderReflectorImpl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include "../../common/UtilityStructsInternal.hpp"
#include <filesystem>
#include <iostream>

namespace st
{
    namespace fs = std::filesystem;

    ShaderPackImpl::ShaderPackImpl(const char* shader_pack_file_path, Session& error_session) :
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

    void ShaderPackImpl::processShaderStages()
    {
        auto& ftracker = ShaderFileTracker::GetFileTracker();
        const std::string working_dir_str = workingDir.string();
        const std::vector<std::string> base_includes{ working_dir_str };

        for (auto& stage : filePack->stages)
        {
            processors.emplace(stage.second, std::make_unique<ShaderStageProcessor>(stage.second, filePack.get()));
            fs::path body_path = fs::canonical(workingDir / fs::path(stage.first));
            if (!fs::exists(body_path))
            {
                // Can't launch for this shader because we have an invalid body path, but let's launch for the others so we can get as many errors as possible
                errorSession.AddError(
                    this,
                    ShaderToolsErrorSource::Filesystem,
                    ShaderToolsErrorCode::FilesystemPathDoesNotExist,
                    stage.first.c_str());
            }
            else
            {
                ftracker.BodyPaths.emplace(stage.second, body_path);
                processorFutures.emplace(stage.second,
                    std::async(std::launch::async,
                               &ShaderStageProcessor::Process,
                               processors.at(stage.second).get(),
                               body_path.string(),
                               filePack->stageExtensions[stage.second],
                               base_includes));
            }
        }
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
            resourceGroups.emplace(entry.first, std::make_unique<ResourceGroup>(filePack.get(), entry.first.c_str()));
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

        groups.emplace(name, std::make_unique<Shader>(name.c_str(), shaders.size(), shaders.data(), filePack.get()));
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
                    errorSession.AddError(
                        this,
                        ShaderToolsErrorSource::ShaderPack,
                        error,
                        nullptr);
                }
            }
        }
    }

}
