#include "core/ResourceGroup.hpp"
#include "../lua/ResourceFile.hpp"
#include <unordered_map>
#include <string>
#include <set>
#include <algorithm>

namespace st {

    class ResourceGroupImpl {
    public:
        ResourceGroupImpl(ResourceFile* resource_file, const char* group_name);
        ~ResourceGroupImpl() = default;
        std::string name;
        std::vector<ShaderResource> resources;
        std::set<std::string> tags;
        std::set<std::string> usedByShaders;
    };

    ResourceGroupImpl::ResourceGroupImpl(ResourceFile* resource_file, const char* group_name) : name(group_name),
        resources(resource_file->setResources.at(group_name)) {
        if (resource_file->groupTags.count(name) != 0) {
            tags = resource_file->groupTags.at(name);
        }
    }

    ResourceGroup::ResourceGroup(ResourceFile* resource_file, const char* group_name) : 
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

    const char* ResourceGroup::Name() const noexcept {
        return impl->name.c_str();
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

    void ResourceGroup::SetName(const char* _name) {
        impl->name = _name;
    }

    void ResourceGroup::UsedByGroup(const char* new_group) {
        impl->usedByShaders.emplace(new_group);
    }

    void ResourceGroup::SetTags(const size_t num_tags, const char** tags) {
        std::vector<const char*> tags_buffer{ tags, tags + num_tags };
        for (auto& str : tags_buffer) {
            impl->tags.emplace(str);
        }
    }

}
