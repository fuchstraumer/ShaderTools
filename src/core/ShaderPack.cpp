#include "core/ShaderPack.hpp"
#include "core/ShaderGroup.hpp"
#include "../lua/LuaEnvironment.hpp"

#include "core/ShaderResource.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "../parser/BindingGeneratorImpl.hpp"
#include "common/UtilityStructs.hpp"
#include "shader_pack_file.hpp"
#include "easyloggingpp/src/easylogging++.h"
INITIALIZE_NULL_EASYLOGGINGPP
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
        void createGroups();
        void createSingleGroup(const std::string& name, const std::map<VkShaderStageFlagBits, std::string>& shader_map);
        void setDescriptorTypeCounts() const;

        std::vector<std::set<ShaderResource>> resources;
        std::unordered_map<std::string, std::vector<size_t>> groupSetIndices;
        std::unordered_map<std::string, std::unique_ptr<ShaderGroup>> groups;
        std::unique_ptr<shader_pack_file_t> filePack;
        std::experimental::filesystem::path workingDir;
        std::mutex guardMutex;
        ResourceFile* rsrcFile;
        mutable descriptor_type_counts_t typeCounts;
    };


    ShaderPackImpl::ShaderPackImpl(const char * shader_pack_file_path) : filePack(std::make_unique<shader_pack_file_t>(shader_pack_file_path)), workingDir(shader_pack_file_path) {
        namespace fs = std::experimental::filesystem;
        workingDir = fs::absolute(workingDir);
        workingDir = workingDir.remove_filename();
        createGroups();
        setDescriptorTypeCounts();
    }

    void ShaderPackImpl::createGroups() {
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
            groups.emplace(group.first, std::make_unique<ShaderGroup>(group.first.c_str(), resource_path_str.c_str(), base_includes.size(), base_includes.data()));
            createSingleGroup(group.first, group.second);
        }

        resource_path = fs::absolute(resource_path);
        auto& ftracker = ShaderFileTracker::GetFileTracker();
        rsrcFile = ftracker.ResourceScripts.at(resource_path.string()).get();
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


            groups.at(name)->AddShader(shader_name_str.c_str(), shader_path_str.c_str(), shader.first);
        }
    }

    void ShaderPackImpl::setDescriptorTypeCounts() const {
        const auto& sets = rsrcFile->GetAllResources();
        for (const auto& resource_set : sets) {
            for (const auto& resource : resource_set.second) {
                switch (resource.GetType()) {
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

    ShaderGroup * ShaderPack::GetShaderGroup(const char * name) const {
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
        const auto& sets = impl->rsrcFile->GetAllResources();
        names.SetNumStrings(sets.size());
        size_t i = 0;
        for (const auto& set : sets) {
            names.Strings[i] = strdup(set.first.c_str());
            ++i;
        }

        return names;
    }

    void ShaderPack::GetResourceGroupPointers(const char * name, size_t * num_resources, const ShaderResource** pointers) {
        const auto& sets = impl->rsrcFile->GetAllResources();
        auto iter = sets.find(std::string(name));
        if (iter != sets.cend()) {
            *num_resources = iter->second.size();
            if (pointers != nullptr) {
                size_t i = 0;
                for (auto& resource : iter->second) {
                    pointers[i] = &resource;
                    ++i;
                }
                return;
            }
        }
        else {
            *num_resources = 0;
            return;
        }
    }

    void ShaderPack::CopyShaderResources(const char * name, size_t * num_resources, ShaderResource * dest_array) {
        const auto& sets = impl->rsrcFile->GetAllResources();
        auto iter = sets.find(std::string(name));
        if (iter != sets.cend()) {
            *num_resources = iter->second.size();
            if (dest_array != nullptr) {
                std::vector<ShaderResource> resources;
                for (const auto& rsrc : iter->second) {
                    resources.emplace_back(rsrc);
                }
                std::copy(resources.begin(), resources.end(), dest_array);
                return;
            }
        }
        else {
            *num_resources = 0;
            return;
        }
    }

    const descriptor_type_counts_t& ShaderPack::GetTotalDescriptorTypeCounts() const {
        return impl->typeCounts;
    }

    const ShaderResource * ShaderPack::GetResource(const char* rsrc_name) {
        const ShaderResource* result = impl->rsrcFile->FindResource(rsrc_name);
        LOG_IF(result == nullptr, WARNING) << "Couldn't find requested resource" << rsrc_name << " in ShaderPack's resource script data.";
        return result;
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