#include "core/Shader.hpp"
#include "impl/ShaderImpl.hpp"
#include "resources/ShaderResource.hpp"
#include "resources/ResourceUsage.hpp"
#include "../common/impl/SessionImpl.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "reflection/ShaderReflector.hpp"
#include "../reflection/impl/ShaderReflectorImpl.hpp"
#include <unordered_set>
#include <iostream>

namespace st
{

    size_t Shader::GetNumSetsRequired() const
    {
        return static_cast<size_t>(impl->reflector->GetNumSets());
    }

    size_t Shader::GetIndex() const noexcept
    {
        return impl->idx;
    }

    void Shader::SetIndex(size_t _idx)
    {
        impl->idx = std::move(_idx);
    }

    void Shader::SetTags(const size_t num_tags, const char** tag_strings)
    {
        for (size_t i = 0; i < num_tags; ++i)
        {
            impl->tags.emplace_back(tag_strings[i]);
        }
    }

    ShaderReflectorImpl* Shader::GetShaderReflectorImpl()
    {
        return impl->reflector->GetImpl();
    }

    const ShaderReflectorImpl* Shader::GetShaderReflectorImpl() const
    {
        return impl->reflector->GetImpl();
    }

    Shader::Shader(
        const char* group_name,
        const size_t num_stages,
        const ShaderStage* stages,
        yamlFile* resource_file,
        SessionImpl* error_session_impl) : impl(std::make_unique<ShaderGroupImpl>(group_name, resource_file, error_session_impl->parent))
    {
        for (size_t i = 0; i < num_stages; ++i)
        {
            impl->addShaderStage(stages[i]);
        }

    }

    Shader::~Shader() {}

    Shader::Shader(Shader && other) noexcept : impl(std::move(other.impl)) {}

    Shader& Shader::operator=(Shader&& other) noexcept
    {
        impl = std::move(other.impl);
        return *this;
    }

    void Shader::GetInputAttributes(const VkShaderStageFlags stage, size_t* num_attrs, VertexAttributeInfo* attributes) const
    {
        impl->reflector->GetInputAttributes(stage, num_attrs, attributes);
    }

    void Shader::GetOutputAttributes(const VkShaderStageFlags stage, size_t* num_attrs, VertexAttributeInfo* attributes) const
    {
        impl->reflector->GetOutputAttributes(stage, num_attrs, attributes);
    }

    PushConstantInfo Shader::GetPushConstantInfo(const VkShaderStageFlags stage) const {
        return impl->reflector->GetStagePushConstantInfo(stage);
    }

    void Shader::GetShaderStages(size_t * num_stages, ShaderStage* stages) const {
        *num_stages = impl->stHandles.size();
        if (stages != nullptr) {
            std::vector<ShaderStage> stages_buffer;
            for (const auto& stage : impl->stHandles)
            {
                stages_buffer.emplace_back(stage);
            }
            std::copy(std::begin(stages_buffer), std::end(stages_buffer), stages);
        }
    }

    ShaderToolsErrorCode Shader::GetShaderBinary(const ShaderStage& handle, size_t* binary_size, uint32_t* dest_binary_ptr) const
    {
        if (!impl->stHandles.contains(handle))
        {
            *binary_size = 0;
            return ShaderToolsErrorCode::ShaderDoesNotContainGivenHandle;
        }
        else
        {

            ReadRequest binaryReadReq{ ReadRequest::Type::FindShaderBinary, handle };
            ReadRequestResult readResult = MakeFileTrackerReadRequest(binaryReadReq);

            if (readResult.has_value())
            {
                const std::vector<uint32_t>& binary_vec_ref = std::get<std::vector<uint32_t>>(*readResult);
                *binary_size = binary_vec_ref.size();
                if (dest_binary_ptr != nullptr)
                {
                    std::vector<uint32_t> binary_vec_copy = std::get<std::vector<uint32_t>>(*readResult);
                    std::copy(binary_vec_copy.begin(), binary_vec_copy.end(), dest_binary_ptr);
                }

                return ShaderToolsErrorCode::Success;
            }
            else
            {
                return readResult.error();
            }

        }

    }

    void Shader::GetSetLayoutBindings(const size_t& set_idx, size_t* num_bindings, VkDescriptorSetLayoutBinding* bindings) const
    {
        const auto* b_impl = GetShaderReflectorImpl();

        auto iter = b_impl->sortedSets.find(static_cast<uint32_t>(set_idx));
        if (iter != b_impl->sortedSets.cend())
        {
            *num_bindings = iter->second.Members.size();
            if (bindings != nullptr)
            {
                std::vector<VkDescriptorSetLayoutBinding> bindings_vec;
                for (auto& member : iter->second.Members)
                {
                    bindings_vec.emplace_back((VkDescriptorSetLayoutBinding)member.second);
                }
                std::copy(bindings_vec.begin(), bindings_vec.end(), bindings);
            }
        }
        else
        {
            *num_bindings = 0;
        }
    }

    void Shader::GetSpecializationConstants(size_t* num_constants, SpecializationConstant* constants) const
    {
        const ShaderReflectorImpl* b_impl = GetShaderReflectorImpl();
        if (!b_impl->specializationConstants.empty())
        {
            *num_constants = b_impl->specializationConstants.size();
            if (constants != nullptr)
            {
                std::vector<SpecializationConstant> constant_vec;
                for (const auto& entry : b_impl->specializationConstants)
                {
                    constant_vec.emplace_back(entry.second);
                }
                std::copy(constant_vec.begin(), constant_vec.end(), constants);
            }
        }
        else
        {
            *num_constants = 0;
        }
    }

    void Shader::GetResourceUsages(const size_t& _set_idx, size_t* num_resources, ResourceUsage* resources) const
    {
        const ShaderReflectorImpl* b_impl = GetShaderReflectorImpl();
        const uint32_t set_idx = static_cast<uint32_t>(_set_idx);
        if (b_impl->sortedSets.count(set_idx) != 0)
        {
            if (b_impl->sortedSets.at(set_idx).Members.empty())
            {
                *num_resources = 0;
                return;
            }
            *num_resources = b_impl->sortedSets.at(set_idx).Members.size();
            if (resources != nullptr)
            {
                std::vector<ResourceUsage> resources_vec;
                for (const auto& rsrc : b_impl->sortedSets.at(set_idx).Members)
                {
                    resources_vec.emplace_back(rsrc.second);
                }
                std::copy(resources_vec.begin(), resources_vec.end(), resources);
            }
        }
    }

    VkShaderStageFlags Shader::Stages() const noexcept
    {
        VkShaderStageFlags result = 0;
        for (auto& shader : impl->stHandles)
        {
            result |= shader.stageBits;
        }
        return result;
    }

    bool Shader::OptimizationEnabled(const ShaderStage& handle) const noexcept
    {
        if (auto iter = impl->optimizationEnabled.find(handle); iter != std::end(impl->optimizationEnabled))
        {
            return iter->second;
        }
        else
        {
            return false;
        }
    }

    uint32_t Shader::ResourceGroupSetIdx(const char* name) const
    {
        const auto* refl_impl = GetShaderReflectorImpl();
        auto iter = refl_impl->resourceGroupSetIndices.find(name);
        if (iter == std::end(refl_impl->resourceGroupSetIndices))
        {
            return std::numeric_limits<uint32_t>::max();
        }
        else
        {
            return iter->second;
        }
    }

    dll_retrieved_strings_t Shader::GetTags() const
    {
        dll_retrieved_strings_t results;
        results.SetNumStrings(impl->tags.size());
        for (size_t i = 0; i < impl->tags.size(); ++i)
        {
            results.Strings[i] = strdup(impl->tags[i].c_str());
        }
        return results;
    }

    dll_retrieved_strings_t Shader::GetSetResourceNames(const uint32_t set_idx) const
    {
        const auto& b_impl = GetShaderReflectorImpl();
        auto iter = b_impl->sortedSets.find(set_idx);

        if (iter != b_impl->sortedSets.cend())
        {
            dll_retrieved_strings_t results;
            results.SetNumStrings(iter->second.Members.size());
            size_t i = 0;
            for (auto& member : iter->second.Members)
            {
                results.Strings[i] = strdup(member.second.BackingResource()->Name());
                ++i;
            }
            return results;
        }
        else
        {
            return dll_retrieved_strings_t();
        }

    }

    dll_retrieved_strings_t Shader::GetUsedResourceBlocks() const
    {

        const auto* refl_impl = GetShaderReflectorImpl();
        dll_retrieved_strings_t results;
        results.SetNumStrings(refl_impl->usedResourceGroupNames.size());

        size_t idx = 0;
        for (const auto& str : refl_impl->usedResourceGroupNames)
        {
            results.Strings[idx] = strdup(str.c_str());
            ++idx;
        }

        return results;
    }

}
