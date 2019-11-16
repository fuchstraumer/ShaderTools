#include "resources/ResourceGroup.hpp"
#include "../parser/yamlFile.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "common/ShaderStage.hpp"
#include <unordered_map>
#include <string>
#include <set>
#include <algorithm>

namespace st {

    class ResourceGroupImpl {
    public:
        ResourceGroupImpl(yamlFile* resource_file, const char* group_name);
        ~ResourceGroupImpl() = default;

        bool hasResource(const char* str) const;
        std::string name;
        std::vector<ShaderResource> resources;
        descriptor_type_counts_t descriptorCounts;
        // what index this group is bound at in the given shader
        std::unordered_map<ShaderStage, uint32_t> stageSetIndices;
        std::vector<std::string> tags;
        std::set<std::string> usedByShaders;
    };

    ResourceGroupImpl::ResourceGroupImpl(yamlFile* resource_file, const char* group_name) : name(group_name),
        resources(resource_file->resourceGroups.at(group_name)) {
        if (resource_file->groupTags.count(name) != 0) {
            tags = resource_file->groupTags.at(name);
        }

        for (const auto& rsrc : resources) {
            switch (rsrc.DescriptorType()) {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
                descriptorCounts.Samplers++;
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                descriptorCounts.CombinedImageSamplers++;
                break;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                descriptorCounts.SampledImages++;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                descriptorCounts.StorageImages++;
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                descriptorCounts.UniformTexelBuffers++;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                descriptorCounts.StorageTexelBuffers++;
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                descriptorCounts.UniformBuffers++;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                descriptorCounts.StorageBuffers++;
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                descriptorCounts.UniformBuffersDynamic++;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                descriptorCounts.StorageBuffersDynamic++;
                break;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                descriptorCounts.InputAttachments++;
                break;
            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
                descriptorCounts.InlineUniformBlockEXT++;
                break;
            //case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX:
            //    descriptorCounts.AccelerationStructureNVX++;
            //    break;
            default:
                throw std::domain_error("Invalid VK_DESCRIPTOR_TYPE value for a ShaderResource in a ResourceGroup!");
            }
        }

    }

    bool ResourceGroupImpl::hasResource(const char* str) const {
        for (const auto& rsrc : resources) {
            if (strcmp(str, rsrc.Name()) == 0) {
                return true;
            }
        }
        return false;
    }

    ResourceGroup::ResourceGroup(yamlFile* resource_file, const char* group_name) : 
        impl(std::make_unique<ResourceGroupImpl>(resource_file, group_name)) {}
    
    ResourceGroup::~ResourceGroup() {
        impl.reset();
    }

    dll_retrieved_strings_t ResourceGroup::ResourceNames() const noexcept {
        dll_retrieved_strings_t results;
        results.SetNumStrings(impl->resources.size());
        for (size_t i = 0; i < results.NumStrings; ++i) {
            results.Strings[i] = strdup(impl->resources[i].Name());
        }
        return results;
    }

    dll_retrieved_strings_t ResourceGroup::UsedByGroups() const noexcept {
        dll_retrieved_strings_t results;
        results.SetNumStrings(impl->usedByShaders.size());
        size_t i = 0;
        for (auto& entry : impl->usedByShaders) {
            results.Strings[i] = strdup(entry.c_str());
            ++i;
        }
        return results;
    }

    dll_retrieved_strings_t ResourceGroup::GetTags() const noexcept {
        dll_retrieved_strings_t results;
        results.SetNumStrings(impl->tags.size());
        size_t i = 0;
        for (auto& entry : impl->tags) {
            results.Strings[i] = strdup(entry.c_str());
            ++i;
        }
        return results;
    }

    const descriptor_type_counts_t& ResourceGroup::DescriptorCounts() const noexcept {
        return impl->descriptorCounts;
    }

    const char* ResourceGroup::Name() const noexcept {
        return impl->name.c_str();
    }

    uint32_t ResourceGroup::DescriptorSetIdxInStage(const ShaderStage & handle) const {
        if (impl->stageSetIndices.empty()) {
            // grab the map from the file tracker
            auto& f_tracker = ShaderFileTracker::GetFileTracker();
            impl->stageSetIndices = f_tracker.ResourceGroupSetIndexMaps.at(impl->name);
        }

        auto iter = impl->stageSetIndices.find(handle);
        if (iter != std::cend(impl->stageSetIndices)) {
            return iter->second;
        }
        else {
            return std::numeric_limits<uint32_t>::max();
        }
    }

    ShaderResource* ResourceGroup::operator[](const char* name) noexcept {
        auto iter = std::find_if(impl->resources.begin(), impl->resources.end(), 
            [name](ShaderResource& rsrc){ return rsrc.Name() == name; });
        
        if (iter != std::end(impl->resources)) {
            return &(*iter);
        }
        else {
            return nullptr;
        }
    }

    const ShaderResource* ResourceGroup::operator[](const char* name) const noexcept {
        auto iter = std::find_if(impl->resources.cbegin(), impl->resources.cend(), 
            [name](const ShaderResource& rsrc){ return rsrc.Name() == name; });
        
        if (iter != std::cend(impl->resources)) {
            return &(*iter);
        }
        else {
            return nullptr;
        }
    }

    void ResourceGroup::GetResources(size_t* num_resources, ShaderResource* resources) const {
        *num_resources = impl->resources.size();
        if (resources != nullptr) {
            std::copy(std::begin(impl->resources), std::end(impl->resources), resources);
        }
    }

    void ResourceGroup::GetResourcePtrs(size_t* num_resources, const ShaderResource** resources) const {
        *num_resources = impl->resources.size();
        if (resources != nullptr) {
            size_t i = 0;
            for (auto& rsrc : impl->resources) {
                resources[i] = &rsrc;
                ++i;
            }
        }
    }

    void ResourceGroup::SetName(const char* _name) {
        impl->name = _name;
    }

    void ResourceGroup::UsedByGroup(const char* new_group) {
        impl->usedByShaders.emplace(new_group);
    }

    void ResourceGroup::SetTags(const size_t num_tags, const char** tags) {
        std::vector<const char*> tags_buffer{ tags, tags + num_tags };
        for (auto& str : tags_buffer) {
            impl->tags.emplace_back(str);
        }
    }

}
