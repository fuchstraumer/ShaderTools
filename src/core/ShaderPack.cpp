#include "core/ShaderPack.hpp"
#include "core/Shader.hpp"
#include "../lua/LuaEnvironment.hpp"
#include "core/ShaderResource.hpp"
#include "core/ResourceGroup.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "../reflection/ShaderReflectorImpl.hpp"
#include "generation/ShaderGenerator.hpp"
#include "common/UtilityStructs.hpp"
#include "shader_pack_file.hpp"
#include "easyloggingpp/src/easylogging++.h"
INITIALIZE_EASYLOGGINGPP
#ifdef FindResource
#undef FindResource
#endif
#include "../lua/ResourceFile.hpp"
#include <unordered_map>
#include <experimental/filesystem>
#include <set>
#include <future>
#include <array>

namespace st {

    static int screen_x() {
        return 1920;
    }

    static int screen_y() {
        return 1080;
    }

    static double z_near() {
        return 0.1;
    }

    static double z_far() {
        return 3000.0;
    }

    static double fov_y() {
        return 75.0;
    }

    class ShaderPackImpl {
    public:
        ShaderPackImpl(const ShaderPackImpl&) = delete;
        ShaderPackImpl& operator=(const ShaderPackImpl&) = delete;

        ShaderPackImpl(const char* shader_pack_file_path);
        void createShaders();
        void createResourceGroups();
        void createSingleGroup(const std::string& name, const std::map<VkShaderStageFlagBits, std::string>& shader_map);
        void setDescriptorTypeCounts() const;

        std::unordered_map<std::string, std::unique_ptr<Shader>> groups;
        std::unique_ptr<shader_pack_file_t> filePack;
        std::experimental::filesystem::path workingDir;
        std::mutex guardMutex;
        std::unordered_map<std::string, std::unique_ptr<ResourceGroup>> resourceGroups;
        mutable descriptor_type_counts_t typeCounts;
    private:
        ResourceFile * rsrcFile;
    };


    ShaderPackImpl::ShaderPackImpl(const char * shader_pack_file_path) : filePack(std::make_unique<shader_pack_file_t>(shader_pack_file_path)), workingDir(shader_pack_file_path) {
        namespace fs = std::experimental::filesystem;
        workingDir = fs::absolute(workingDir);
        workingDir = workingDir.remove_filename();
        const std::string dir_string = workingDir.parent_path().string();
        createShaders();
        createResourceGroups();
        setDescriptorTypeCounts();
    }

    void ShaderPackImpl::createShaders() {
        namespace fs = std::experimental::filesystem;

        fs::path resource_path = workingDir / fs::path(filePack->ResourceFileName);
        if (!fs::exists(resource_path)) {
            LOG(ERROR) << "Resource Lua script could not be found using specified path.";
            throw std::runtime_error("Couldn't find resource file using given path.");
        }
        const std::string resource_path_str = resource_path.string();

        const std::string working_dir_str = workingDir.string();
        static const std::array<const char*, 1> base_includes{ working_dir_str.c_str() };

        for (const auto& group : filePack->ShaderGroups) {
            groups.emplace(group.first, std::make_unique<Shader>(group.first.c_str(), resource_path_str.c_str(), base_includes.size(), base_includes.data()));
            createSingleGroup(group.first, group.second);
            groups.at(group.first)->SetIndex(filePack->GroupIndices.at(group.first));
            if (filePack->GroupTags.count(group.first) != 0) {
                const auto& tags = filePack->GroupTags.at(group.first);
                std::vector<const char*> tag_ptrs;
                for (auto& tag : tags) {
                    tag_ptrs.emplace_back(tag.c_str());
                }
                groups.at(group.first)->SetTags(tag_ptrs.size(), tag_ptrs.data());
            }
        }

        resource_path = fs::absolute(resource_path);
        auto& ftracker = ShaderFileTracker::GetFileTracker();
        rsrcFile = ftracker.ResourceScripts.at(resource_path.string()).get();
    }

    void ShaderPackImpl::createResourceGroups() {
        const auto& rsrcs = rsrcFile->GetAllResources();
        for (const auto& entry : rsrcs) {
            resourceGroups.emplace(entry.first, std::make_unique<ResourceGroup>(rsrcFile, entry.first.c_str()));
        }
    }

    void ShaderPackImpl::createSingleGroup(const std::string & name, const std::map<VkShaderStageFlagBits, std::string>& shader_map) {
        namespace fs = std::experimental::filesystem;

        for (const auto& shader : shader_map) {
            std::string shader_name_str = shader.second;
            fs::path shader_path = workingDir / fs::path(shader_name_str);
            if (!fs::exists(shader_path)) {
                LOG(ERROR) << "Shader path given could not be found.";
                throw std::runtime_error("Failed to find shader using given path.");
            }
            const std::string shader_path_str = shader_path.string();

            size_t name_prefix_idx = shader_name_str.find_last_of('/');
            if (name_prefix_idx != std::string::npos) {
                shader_name_str.erase(shader_name_str.begin(), shader_name_str.begin() + name_prefix_idx + 1);
            }

            size_t name_suffix_idx = shader_name_str.find_first_of('.');
            if (name_suffix_idx != std::string::npos) {
                shader_name_str.erase(shader_name_str.begin() + name_suffix_idx, shader_name_str.end());
            }


            groups.at(name)->AddShaderStage(shader_name_str.c_str(), shader_path_str.c_str(), shader.first);
        }
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

    ShaderPack::ShaderPack(const char* fpath) : impl(std::make_unique<ShaderPackImpl>(fpath)) {}

    ShaderPack::~ShaderPack() {}

    const Shader* ShaderPack::GetShaderGroup(const char * name) const {
        if (impl->groups.count(name) != 0) {
            return impl->groups.at(name).get();
        }
        else {
            return nullptr;
        }
    }

    dll_retrieved_strings_t ShaderPack::GetShaderGroupNames() const {
        dll_retrieved_strings_t names;
        names.SetNumStrings(impl->groups.size());
        size_t i = 0;
        for (auto& group : impl->groups) {
            names.Strings[i] = strdup(group.first.c_str());
            ++i;
        }

        return names;
    }

    dll_retrieved_strings_t ShaderPack::GetResourceGroupNames() const {
        dll_retrieved_strings_t names;
        names.SetNumStrings(impl->resourceGroups.size());
        size_t i = 0;
        for (const auto& group : impl->resourceGroups) {
            names.Strings[i] = strdup(group.first.c_str());
            ++i;
        }

        return names;
    }

    const descriptor_type_counts_t& ShaderPack::GetTotalDescriptorTypeCounts() const {
        return impl->typeCounts;
    }

    const ResourceGroup * ShaderPack::GetResourceGroup(const char * name) const {
        auto iter = impl->resourceGroups.find(name);
        if (iter != std::cend(impl->resourceGroups)) {
            return iter->second.get();
        }
        else {
            return nullptr;
        }
    }

    const ShaderResource* ShaderPack::GetResource(const char* rsrc_name) const {

        for (auto& group : impl->resourceGroups) {
            auto find_in_group = [rsrc_name, this](const std::string& group_name)->const ShaderResource* {
                return (*impl->resourceGroups.at(group_name))[rsrc_name];
            };
            const ShaderResource* result = find_in_group(group.first);

            if (result != nullptr) {
                return result;
            }
        }
        
        return nullptr;
    }

    engine_environment_callbacks_t & ShaderPack::RetrievalCallbacks() {
        static engine_environment_callbacks_t callbacks = engine_environment_callbacks_t{
            &screen_x,
            &screen_y,
            &z_near,
            &z_far,
            &fov_y
        };
        return callbacks;
    }

}
