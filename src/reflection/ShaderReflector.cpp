#include "reflection/ShaderReflector.hpp"
#include "impl/ShaderReflectorImpl.hpp"
#include "resources/ResourceUsage.hpp"
#include "reflection/ReflectionStructs.hpp"
#include "common/stSession.hpp"

namespace st
{

    ShaderReflector::ShaderReflector(yamlFile* yaml_file, Session& error_session) : impl(std::make_unique<ShaderReflectorImpl>(yaml_file, error_session.GetImpl())) {}

    ShaderReflector::~ShaderReflector() {}

    ShaderReflector::ShaderReflector(ShaderReflector&& other) noexcept : impl(std::move(other.impl)) {}

    ShaderReflector& ShaderReflector::operator=(ShaderReflector&& other) noexcept
    {
        impl = std::move(other.impl);
        other.impl.reset();
        return *this;
    }

    uint32_t ShaderReflector::GetNumSets() const noexcept
    {
        return static_cast<uint32_t>(impl->getNumSets());
    }

    void ShaderReflector::GetShaderResources(const size_t set_idx, size_t* num_resources, ResourceUsage* resources)
    {
        auto iter = impl->sortedSets.find(static_cast<unsigned int>(set_idx));
        if (iter != impl->sortedSets.cend())
        {
            const auto& set = iter->second;
            *num_resources = set.Members.size();

            std::vector<ResourceUsage> resources_copy;
            for (const auto& member : set.Members)
            {
                resources_copy.emplace_back(member.second);
            }

            if (resources != nullptr)
            {
                std::copy(resources_copy.begin(), resources_copy.end(), resources);
            }
        }
        else
        {
            *num_resources = 0;
        }
    }

    void ShaderReflector::GetInputAttributes(const VkShaderStageFlags stage, size_t* num_attrs, VertexAttributeInfo* attributes)
    {
        auto iter = impl->inputAttributes.find(stage);
        if (iter == impl->inputAttributes.end())
        {
            *num_attrs = 0;
            return;
        }
        else
        {
            *num_attrs = iter->second.size();
            if (attributes != nullptr)
            {
                std::copy(std::begin(iter->second), std::end(iter->second), attributes);
            }
        }
    }

    void ShaderReflector::GetOutputAttributes(const VkShaderStageFlags stage, size_t* num_attrs, VertexAttributeInfo* attributes)
    {
        auto iter = impl->outputAttributes.find(stage);
        if (iter == impl->outputAttributes.end())
        {
            *num_attrs = 0;
            return;
        }
        else
        {
            *num_attrs = iter->second.size();
            if (attributes != nullptr)
            {
                std::copy(std::begin(iter->second), std::end(iter->second), attributes);
            }
        }
    }

    PushConstantInfo ShaderReflector::GetStagePushConstantInfo(const VkShaderStageFlags stage) const
    {
        auto iter = impl->pushConstants.find(stage);
        if (iter != std::end(impl->pushConstants))
        {
            return iter->second;
        }
        else
        {
            return PushConstantInfo();
        }
    }

    ShaderReflectorImpl * ShaderReflector::GetImpl()
    {
        return impl.get();
    }

    st::ShaderToolsErrorCode ShaderReflector::ParseBinary(const ShaderStage& shader)
    {
        return impl->parseBinary(shader);
    }
}
