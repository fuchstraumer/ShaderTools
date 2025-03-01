#include "core/ShaderPack.hpp"
#include "impl/ShaderPackImpl.hpp"
#include "common/UtilityStructs.hpp"
#include "resources/ResourceGroup.hpp"

namespace st
{

    ShaderPack::ShaderPack(const char* fpath, Session& session) : impl(std::make_unique<ShaderPackImpl>(fpath, session.GetImpl())) {}

    ShaderPack::~ShaderPack() {}

    const Shader* ShaderPack::GetShaderGroup(const char * name) const
    {
        if (impl->groups.count(name) != 0)
        {
            return impl->groups.at(name).get();
        }
        else
        {
            return nullptr;
        }
    }

    dll_retrieved_strings_t ShaderPack::GetShaderGroupNames() const
    {
        dll_retrieved_strings_t names;
        names.SetNumStrings(impl->groups.size());
        size_t i = 0;
        for (auto& group : impl->groups)
        {
            names.Strings[i] = strdup(group.first.c_str());
            ++i;
        }

        return names;
    }

    dll_retrieved_strings_t ShaderPack::GetResourceGroupNames() const
    {
        dll_retrieved_strings_t names;
        names.SetNumStrings(impl->resourceGroups.size());
        size_t i = 0;
        for (const auto& group : impl->resourceGroups)
        {
            names.Strings[i] = strdup(group.first.c_str());
            ++i;
        }

        return names;
    }

    const descriptor_type_counts_t& ShaderPack::GetTotalDescriptorTypeCounts() const
    {
        return impl->typeCounts;
    }

    const ResourceGroup* ShaderPack::GetResourceGroup(const char * name) const
    {
        auto iter = impl->resourceGroups.find(name);
        if (iter != std::cend(impl->resourceGroups))
        {
            return iter->second.get();
        }
        else
        {
            return nullptr;
        }
    }

    const ShaderResource* ShaderPack::GetResource(const char* rsrc_name) const
    {

        for (auto& group : impl->resourceGroups)
        {
            const ShaderResource* result = (*impl->resourceGroups.at(group.first))[rsrc_name];

            if (result != nullptr)
            {
                return result;
            }
        }

        return nullptr;
    }

}
