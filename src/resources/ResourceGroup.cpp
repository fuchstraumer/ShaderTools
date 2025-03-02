#include "resources/ResourceGroup.hpp"
#include "../parser/yamlFile.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "../common/UtilityStructsInternal.hpp"
#include "../common/impl/SessionImpl.hpp"
#include "common/ShaderStage.hpp"

#include <unordered_map>
#include <string>
#include <set>
#include <algorithm>

namespace st
{

    class ResourceGroupImpl
    {
    public:
        ResourceGroupImpl(yamlFile* resource_file, const char* group_name, SessionImpl* error_session);
        ~ResourceGroupImpl() = default;

        bool hasResource(const char* str) const;
        std::string name;
        std::vector<ShaderResource> resources;
        descriptor_type_counts_t descriptorCounts;
        // what index this group is bound at in the given shader
        std::unordered_map<ShaderStage, uint32_t> stageSetIndices;
        std::vector<std::string> tags;
        std::set<std::string> usedByShaders;
        SessionImpl* errorSession;
    };

    ResourceGroupImpl::ResourceGroupImpl(yamlFile* resource_file, const char* group_name, SessionImpl* error_session) :
        errorSession(error_session),
        name(group_name),
        resources(resource_file->resourceGroups.at(group_name))
    {
        if (resource_file->groupTags.count(name) != 0)
        {
            tags = resource_file->groupTags.at(name);
        }

        for (const auto& rsrc : resources)
        {
            ShaderToolsErrorCode error = CountDescriptorType(rsrc.DescriptorType(), descriptorCounts);
            if (error != ShaderToolsErrorCode::Success)
            {
                errorSession->AddError(this, ShaderToolsErrorSource::ResourceGroup, error, nullptr);
            }
        }

    }

    bool ResourceGroupImpl::hasResource(const char* str) const
    {
        for (const auto& rsrc : resources)
        {
            if (strcmp(str, rsrc.Name()) == 0)
            {
                return true;
            }
        }
        return false;
    }

    ResourceGroup::ResourceGroup(yamlFile* resource_file, const char* group_name, SessionImpl* error_session) :
        impl(std::make_unique<ResourceGroupImpl>(resource_file, group_name, error_session))
    {}

    ResourceGroup::~ResourceGroup()
    {
        impl.reset();
    }

    dll_retrieved_strings_t ResourceGroup::ResourceNames() const noexcept
    {
        dll_retrieved_strings_t results;
        results.SetNumStrings(impl->resources.size());
        for (size_t i = 0; i < results.NumStrings; ++i)
        {
            results.Strings[i] = strdup(impl->resources[i].Name());
        }
        return results;
    }

    dll_retrieved_strings_t ResourceGroup::UsedByGroups() const noexcept
    {
        dll_retrieved_strings_t results;
        results.SetNumStrings(impl->usedByShaders.size());
        size_t i = 0;
        for (const auto& entry : impl->usedByShaders)
        {
            results.Strings[i] = strdup(entry.c_str());
            ++i;
        }
        return results;
    }

    dll_retrieved_strings_t ResourceGroup::GetTags() const noexcept
    {
        dll_retrieved_strings_t results;
        results.SetNumStrings(impl->tags.size());
        size_t i = 0;
        for (const auto& entry : impl->tags)
        {
            results.Strings[i] = strdup(entry.c_str());
            ++i;
        }
        return results;
    }

    const descriptor_type_counts_t& ResourceGroup::DescriptorCounts() const noexcept
    {
        return impl->descriptorCounts;
    }

    const char* ResourceGroup::Name() const noexcept
    {
        return impl->name.c_str();
    }

    uint32_t ResourceGroup::DescriptorSetIdxInStage(const ShaderStage& handle) const
    {
        if (impl->stageSetIndices.empty())
        {
            ReadRequest readReq{ ReadRequest::Type::FindResourceGroupSetIndexMap, handle };
            ReadRequestResult readResult = MakeFileTrackerReadRequest(readReq);
            if (readResult.has_value())
            {
                impl->stageSetIndices = std::get<decltype(impl->stageSetIndices)>(*readResult);
            }
            else
            {
                impl->errorSession->AddError(this, ShaderToolsErrorSource::ResourceGroup, readResult.error(), "ResourceGroup couldn't get SetIndexMap from FileTracker");
                // oh shit how do we log an error
                return std::numeric_limits<uint32_t>::max();
            }
        }

        auto iter = impl->stageSetIndices.find(handle);
        if (iter != std::cend(impl->stageSetIndices))
        {
            return iter->second;
        }
        else
        {
            return std::numeric_limits<uint32_t>::max();
        }
    }

    ShaderResource* ResourceGroup::operator[](const char* name) noexcept
    {
        auto iter = std::find_if(impl->resources.begin(), impl->resources.end(),
            [name](ShaderResource& rsrc){ return rsrc.Name() == name; });

        if (iter != std::end(impl->resources))
        {
            return &(*iter);
        }
        else
        {
            impl->errorSession->AddError(this, ShaderToolsErrorSource::ResourceGroup, ShaderToolsErrorCode::ResourceNotFound, name);
            return nullptr;
        }
    }

    const ShaderResource* ResourceGroup::operator[](const char* name) const noexcept
    {
        auto iter = std::find_if(impl->resources.cbegin(), impl->resources.cend(),
            [name](const ShaderResource& rsrc){ return rsrc.Name() == name; });

        if (iter != std::cend(impl->resources))
        {
            return &(*iter);
        }
        else
        {
            impl->errorSession->AddError(this, ShaderToolsErrorSource::ResourceGroup, ShaderToolsErrorCode::ResourceNotFound, name);
            return nullptr;
        }
    }

    void ResourceGroup::GetResources(size_t* num_resources, ShaderResource* resources) const
    {
        *num_resources = impl->resources.size();
        if (resources != nullptr)
        {
            std::copy(std::begin(impl->resources), std::end(impl->resources), resources);
        }
    }

    void ResourceGroup::GetResourcePtrs(size_t* num_resources, const ShaderResource** resources) const
    {
        *num_resources = impl->resources.size();
        if (resources != nullptr)
        {
            size_t i = 0;
            for (auto& rsrc : impl->resources)
            {
                resources[i] = &rsrc;
                ++i;
            }
        }
    }

    void ResourceGroup::SetName(const char* _name)
    {
        impl->name = _name;
    }

    void ResourceGroup::UsedByGroup(const char* new_group)
    {
        impl->usedByShaders.emplace(new_group);
    }

    void ResourceGroup::SetTags(const size_t num_tags, const char** tags)
    {
        std::vector<const char*> tags_buffer{ tags, tags + num_tags };
        for (auto& str : tags_buffer)
        {
            impl->tags.emplace_back(str);
        }
    }

}
