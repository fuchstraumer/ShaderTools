#include "ShaderPackImpl.hpp"
#include "ShaderImpl.hpp"
#include "../../lua/LuaEnvironment.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include "../../util/ShaderPackBinary.hpp"
#include "../../reflection/impl/ShaderReflectorImpl.hpp"
#include "../../lua/ResourceFile.hpp"
#include "ShaderStageProcessor.hpp"
#include "core/ResourceGroup.hpp"
#include "easyloggingpp/src/easylogging++.h"
#include <array>

#if !defined(ST_BUILDING_STATIC)
INITIALIZE_EASYLOGGINGPP
#endif
// Imported from windows headers included by easyloggingpp
#ifdef FindResource
#undef FindResource
#endif

namespace st {

    namespace fs = std::experimental::filesystem;

    ShaderPackImpl::ShaderPackImpl(const char * shader_pack_file_path) : filePack(std::make_unique<ShaderPackScript>(shader_pack_file_path)), workingDir(shader_pack_file_path) {

        workingDir = fs::canonical(workingDir);
        packScriptPath = workingDir.string();
        workingDir = workingDir.remove_filename();

        executeResourceScript();
        processShaderStages();

        createShaders();
        createResourceGroups();
        setDescriptorTypeCounts();
    }

    ShaderPackImpl::ShaderPackImpl(ShaderPackBinary * binary_data) {
        detail::LoadPackFromBinary(this, binary_data);
        executeResourceScript();
        processShaderStages();
        createShaders();
        createResourceGroups();
        setDescriptorTypeCounts();
    }

    ShaderPackImpl::~ShaderPackImpl() {}

    void ShaderPackImpl::createPackScript(const char* fname) {
        filePack = std::make_unique<ShaderPackScript>(fname);
    }

    void ShaderPackImpl::executeResourceScript() {

        fs::path resource_path = workingDir / fs::path(filePack->ResourceFileName);
        
        if (!fs::exists(resource_path)) {
            LOG(ERROR) << "Resource Lua script could not be found using specified path.";
            throw std::runtime_error("Couldn't find resource file using given path.");
        }

        resourceScriptPath = fs::canonical(resource_path).string();
        const std::string resource_path_str = resource_path.string();
        auto& ftracker = ShaderFileTracker::GetFileTracker();
        if (!ftracker.FindResourceScript(resource_path_str, &rsrcFile)) {
            LOG(ERROR) << "Failed to fully execute or find executed resource script!";
            throw std::runtime_error("Couldn't find or execute resource script.");
        }

    }

    void ShaderPackImpl::processShaderStages() {

        auto& ftracker = ShaderFileTracker::GetFileTracker();
        const std::string working_dir_str = workingDir.string();
        const std::vector<std::string> base_includes{ working_dir_str };

        for (auto& stage : filePack->Stages) {
            processors.emplace(stage.second, std::make_unique<ShaderStageProcessor>(stage.second, rsrcFile));
            fs::path body_path = fs::canonical(workingDir / fs::path(stage.first));
            if (!fs::exists(body_path)) {
                throw std::runtime_error("Path for shader stage to be generated does not exist!");
            }
            ftracker.BodyPaths.emplace(stage.second, body_path);
            processorFutures.emplace(stage.second, std::async(std::launch::async, &ShaderStageProcessor::Process, processors.at(stage.second).get(), body_path.string(), filePack->StageExtensions[stage.second], base_includes));
        }

    }

    void ShaderPackImpl::createShaders() {

        for (const auto& group : filePack->ShaderGroups) {
            std::vector<ShaderStage> stages{};
            std::unique_copy(group.second.cbegin(), group.second.cend(), std::back_inserter(stages));
            createSingleGroup(group.first, stages);
        }

    }

    void ShaderPackImpl::createResourceGroups() {
        const auto& rsrcs = rsrcFile->GetAllResources();
        for (const auto& entry : rsrcs) {
            resourceGroups.emplace(entry.first, std::make_unique<ResourceGroup>(rsrcFile, entry.first.c_str()));
        }
    }

    void ShaderPackImpl::createSingleGroup(const std::string& name, const std::vector<ShaderStage> shaders) {
        
        for (const auto& shader : shaders) {
            // make sure processing is done
            if (processorFutures.count(shader)) {
                processorFutures.at(shader).get();
                processorFutures.erase(shader);
            }
        }

        groups.emplace(name, std::make_unique<Shader>(name.c_str(), shaders.size(), shaders.data(), resourceScriptPath.c_str()));
    }

    void ShaderPackImpl::setDescriptorTypeCounts() const {
        const auto& sets = rsrcFile->GetAllResources();
        for (const auto& resource_set : sets) {
            for (const auto& resource : resource_set.second) {
                switch (resource.DescriptorType()) {
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                    typeCounts.Samplers++;
                    break;
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    typeCounts.CombinedImageSamplers++;
                    break;
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    typeCounts.SampledImages++;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    typeCounts.SampledImages++;
                    break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                    typeCounts.UniformTexelBuffers++;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                    typeCounts.StorageTexelBuffers++;
                    break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    typeCounts.UniformBuffers++;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    typeCounts.StorageBuffers++;
                    break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                    typeCounts.UniformBuffersDynamic++;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                    typeCounts.StorageBuffersDynamic++;
                    break;
                default:
                    LOG(ERROR) << "Got invalid VkDescriptor type value when counting up descriptors of each type!";
                    throw std::domain_error("Invalid VkDescriptorType enum value.");
                }
            }
        }
    }

}
