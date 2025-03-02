#include "ShaderReflectorImpl.hpp"
#include "../../common/impl/SessionImpl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include "../../parser/yamlFile.hpp"
#include "resources/ShaderResource.hpp"
#include "resources/ResourceUsage.hpp"
#include "../../util/ResourceFormats.hpp"
#include <array>
#include <iostream>
#include <fstream>
#include <algorithm>

namespace st
{

    constexpr VkDescriptorType ConvertSpvReflectDescriptorType(SpvReflectDescriptorType type) noexcept
    {
        switch (type)
        {
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                return VK_DESCRIPTOR_TYPE_SAMPLER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            default:
                return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        };
    }

    constexpr uint32_t GetFormatSize(SpvReflectFormat format) noexcept
    {
        switch (format)
        {
            case SPV_REFLECT_FORMAT_R16_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R16_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R16_SFLOAT:
                return 2;
            case SPV_REFLECT_FORMAT_R16G16_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R16G16_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R16G16_SFLOAT:
                return 4;
            case SPV_REFLECT_FORMAT_R16G16B16_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R16G16B16_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:
                return 6;
            case SPV_REFLECT_FORMAT_R16G16B16A16_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R16G16B16A16_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT:
                return 8;
            case SPV_REFLECT_FORMAT_R32_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R32_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R32_SFLOAT:
                return 4;
            case SPV_REFLECT_FORMAT_R32G32_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R32G32_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
                return 8;
            case SPV_REFLECT_FORMAT_R32G32B32_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R32G32B32_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
                return 12;
            case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
                return 16;
            case SPV_REFLECT_FORMAT_R64_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R64_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R64_SFLOAT:
                return 8;
            case SPV_REFLECT_FORMAT_R64G64_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R64G64_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R64G64_SFLOAT:
                return 16;
            case SPV_REFLECT_FORMAT_R64G64B64_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R64G64B64_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT:
                return 24;
            case SPV_REFLECT_FORMAT_R64G64B64A64_UINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R64G64B64A64_SINT:
                [[fallthrough]];
            case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT:
                return 32;
            default:
                return 4;
        };
    }

    constexpr bool IsBufferType(const VkDescriptorType& type) noexcept
    {
        return (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) || (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
            (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) || (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
    }

    constexpr bool IsReadOnlyDescriptorType(const SpvReflectDescriptorType type)
    {
        if (type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
            type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
            type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    constexpr access_modifier DetermineAccessModifier(const SpvReflectDescriptorBinding* binding) noexcept
    {
        access_modifier modifier = access_modifier::ReadWrite;

        if (IsReadOnlyDescriptorType(binding->descriptor_type))
        {
            return access_modifier::Read;
        }
        else
        {
            if (binding->type_description && binding->type_description->decoration_flags)
            {
                if (binding->type_description->decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE)
                {
                    return access_modifier::Read;
                }
                else if (binding->type_description->decoration_flags & SPV_REFLECT_DECORATION_NON_READABLE)
                {
                    return access_modifier::Write;
                }
            }
        }

        return access_modifier::ReadWrite;
    }

    constexpr bool StageRequiresVertexInputs(const VkShaderStageFlags stage_flags)
    {
        return (stage_flags & VK_SHADER_STAGE_VERTEX_BIT) ||
               (stage_flags & VK_SHADER_STAGE_GEOMETRY_BIT) ||
               (stage_flags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ||
               (stage_flags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) ||
               (stage_flags & VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    constexpr bool StageRequiresVertexOutputs(const VkShaderStageFlags stage_flags)
    {
        return (stage_flags & VK_SHADER_STAGE_VERTEX_BIT) ||
               (stage_flags & VK_SHADER_STAGE_GEOMETRY_BIT) ||
               (stage_flags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ||
               (stage_flags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    }

    ShaderResourceSubObject CreateSubobject(const SpvReflectBlockVariable& block_variable)
    {
        ShaderResourceSubObject result;
        result.SetName(block_variable.name);
        result.Offset = block_variable.offset;
        result.Size = block_variable.size;
        if (block_variable.type_description && block_variable.type_description->type_name)
        {
            result.SetType(block_variable.type_description->type_name);
        }
        else
        {
            result.SetType(spvReflect_TypeToString(block_variable.type_description->type_flags));
        }
        result.isComplex = block_variable.type_description && block_variable.type_description->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT;
        return result;
    }

    std::vector<ShaderResourceSubObject> CreateSubobjects(const uint32_t num_subobjects, const SpvReflectBlockVariable* variables)
    {
        std::vector<ShaderResourceSubObject> result;
        if (num_subobjects == 0)
        {
            return result;
        }

        result.reserve(num_subobjects);

        for (uint32_t i = 0; i < num_subobjects; ++i)
        {
            result.emplace_back(CreateSubobject(variables[i]));
        }

        return result;
    }

    ShaderReflectorImpl::ShaderReflectorImpl(yamlFile* yaml_file, SessionImpl* error_session) noexcept :
        rsrcFile(yaml_file),
        errorSession(error_session),
        spvReflectModule{ nullptr, &DestroySpvReflectShaderModule }
    {
    }

    ShaderReflectorImpl::ShaderReflectorImpl(ShaderReflectorImpl&& other) noexcept :
        descriptorSets(std::move(other.descriptorSets)),
        sortedSets(std::move(other.sortedSets)),
        pushConstants(std::move(other.pushConstants)),
        rsrcFile{ other.rsrcFile },
        errorSession{ other.errorSession },
        spvReflectModule{ std::move(other.spvReflectModule) }
    {
        spvReflectModule = nullptr;
    }


    ShaderReflectorImpl::~ShaderReflectorImpl()
    {
    }

    ShaderReflectorImpl& ShaderReflectorImpl::operator=(ShaderReflectorImpl&& other) noexcept
    {
        descriptorSets = std::move(other.descriptorSets);
        sortedSets = std::move(other.sortedSets);
        pushConstants = std::move(other.pushConstants);
        rsrcFile = other.rsrcFile;
        errorSession = std::move(other.errorSession);
        return *this;
    }

    std::vector<VertexAttributeInfo> ShaderReflectorImpl::parseInterfaceVariables(InterfaceVariableType type)
    {
       uint32_t count = 0;
       SpvReflectResult result = SPV_REFLECT_RESULT_SUCCESS;
       switch (type)
       {
        case InterfaceVariableType::Input:
            result = spvReflectEnumerateInputVariables(spvReflectModule.get(), &count, nullptr);
            break;
        case InterfaceVariableType::Output:
            result = spvReflectEnumerateOutputVariables(spvReflectModule.get(), &count, nullptr);
            break;
       }

        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            std::string error_message = std::format("Failed to get count of {} interface variables", type == InterfaceVariableType::Input ? "input" : "output");
            errorSession->AddError(
                this,
                ShaderToolsErrorSource::Reflection,
                ShaderToolsErrorCode::SpvReflectErrorsStart,
                error_message.c_str());
            return {};
        }
        else if (count == 0)
        {
            return {};
        }

        std::vector<SpvReflectInterfaceVariable*> interface_variables(count);
        switch (type)
        {
        case InterfaceVariableType::Input:
            result = spvReflectEnumerateInputVariables(spvReflectModule.get(), &count, interface_variables.data());
            break;
        case InterfaceVariableType::Output:
            result = spvReflectEnumerateOutputVariables(spvReflectModule.get(), &count, interface_variables.data());
            break;
        }

        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            errorSession->AddError(this, ShaderToolsErrorSource::Reflection, ShaderToolsErrorCode::SpvReflectErrorsStart, "Failed to get actual interface variables from SpvReflect.");
            return {};
        }

        std::vector<VertexAttributeInfo> attributes(interface_variables.size());
        uint32_t running_offset = 0;
        for (const auto& interface_variable : interface_variables)
        {
            VertexAttributeInfo attr_info;
            if (interface_variable->name && std::string(interface_variable->name).empty())
            {
                // Check to see if the type information holds the name, and see if it's just our gl_PerVertex declaration
                // If that's the case, we have to tear this out of the results as that's not actually something we care about
                // (also, these are placed at the front of interface_variables but location indices for others start at 0 after this member)
                if (interface_variable->type_description && interface_variable->type_description->type_name)
                {
                    std::string type_name = interface_variable->type_description->type_name;
                    if (type_name == std::string("gl_PerVertex"))
                    {
                        attributes.resize(attributes.size() - 1);
                        continue;
                    }
                }
                else
                {
                    const std::string error_message("Found an empty name string for an interface variable, and it wasn't an expected built in value!");
                    errorSession->AddError(this, ShaderToolsErrorSource::Reflection, ShaderToolsErrorCode::SpvReflectErrorsStart, error_message.c_str());
                    return {};
                }
            }
            else if (interface_variable->built_in != -1)
            {
                attributes.resize(attributes.size() - 1);
                continue;
            }
            else if (!interface_variable->name)
            {
                // If we still don't have a name, than that's definitely a genuine error.
                std::string error_message = std::format("Input variable at location {} has no name", interface_variable->location);
                errorSession->AddError(
                    this,
                    ShaderToolsErrorSource::Reflection,
                    ShaderToolsErrorCode::SpvReflectErrorsStart,
                    error_message.c_str());
                return {};
            }

            attr_info.SetName(interface_variable->name);
            attr_info.SetLocation(interface_variable->location);
            attr_info.SetOffset(running_offset);
            attr_info.SetFormatFromSpvReflectFlags(interface_variable->format);

            running_offset += GetFormatSize(interface_variable->format);
            attributes[interface_variable->location] = attr_info;

        }

        return attributes;
    }

    ShaderToolsErrorCode ShaderReflectorImpl::parseDescriptorBindings(const ShaderStage& shader_handle)
    {
        uint32_t count = 0;
        SpvReflectResult result = spvReflectEnumerateDescriptorBindings(spvReflectModule.get(), &count, nullptr);
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            errorSession->AddError(
                this,
                ShaderToolsErrorSource::Reflection,
                ShaderToolsErrorCode::SpvReflectErrorsStart,
                "Failed to enumerate descriptor sets");
            return ShaderToolsErrorCode::SpvReflectErrorsStart;
        }
        else if (count == 0)
        {
            return ShaderToolsErrorCode::Success;
        }
        
        std::vector<SpvReflectDescriptorBinding*> descriptor_bindings(count);
        result = spvReflectEnumerateDescriptorBindings(spvReflectModule.get(), &count, descriptor_bindings.data());
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            errorSession->AddError(
                this,
                ShaderToolsErrorSource::Reflection,
                ShaderToolsErrorCode::SpvReflectErrorsStart,
                "Failed to enumerate descriptor sets");
            return ShaderToolsErrorCode::SpvReflectErrorsStart;
        }

        for (const SpvReflectDescriptorBinding* descriptor_binding : descriptor_bindings)
        {
            VkDescriptorType descriptor_type = ConvertSpvReflectDescriptorType(descriptor_binding->descriptor_type);

            if (descriptor_binding->name == nullptr)
            {
                std::string error_message = std::format("spvReflect couldn't find name of descriptor at set {} and binding {}", descriptor_binding->set, descriptor_binding->binding);
                errorSession->AddError(
                    this,
                    ShaderToolsErrorSource::Reflection,
                    ShaderToolsErrorCode::SpvReflectErrorsStart,
                    error_message.c_str());
            }

            std::string rsrc_name = descriptor_binding->name;
            if (rsrc_name.empty() && descriptor_binding->type_description && descriptor_binding->type_description->type_name)
            {
                rsrc_name = descriptor_binding->type_description->type_name;
            }

            rsrc_name = GetActualResourceName(rsrc_name);

            const ShaderResource* parent_resource = rsrcFile->FindResource(rsrc_name);
            if (!parent_resource)
            {
                std::string error_message = std::format(
                    "Couldn't find parent resource for descriptor with name {} in set {} at binding {}",
                    rsrc_name,
                    descriptor_binding->set,
                    descriptor_binding->binding);
                errorSession->AddError(
                    this,
                    ShaderToolsErrorSource::Reflection,
                    ShaderToolsErrorCode::ReflectionInvalidResource,
                    error_message.c_str());
            }

            const std::string parent_group_name = parent_resource->ParentGroupName();
            if (usedResourceGroupNames.count(parent_group_name) == 0)
            {
                usedResourceGroupNames.emplace(parent_group_name);
            }

            if (descriptor_binding->binding != parent_resource->BindingIdx())
            {
                std::string error_message = std::format(
                    "Binding index mismatch, spvReflect says binding index is {} but resource is declared to be at binding {}",
                    descriptor_binding->binding, parent_resource->BindingIdx());
                errorSession->AddError(
                    this,
                    ShaderToolsErrorSource::Reflection,
                    ShaderToolsErrorCode::ReflectionInvalidBindingIndex,
                    error_message.c_str());
            }

            if (resourceGroupSetIndices.count(parent_group_name) == 0)
            {
                resourceGroupSetIndices.emplace(parent_group_name, descriptor_binding->set);
            }

            access_modifier modifier = DetermineAccessModifier(descriptor_binding);
            auto iter = tempResources.emplace(
                descriptor_binding->set,
                ResourceUsage(shader_handle, parent_resource, modifier, descriptor_type));

            [[unlikely]]
            if (iter == tempResources.end())
            {
                std::string error_message = std::format("Failed to insert resource {} into tempResources", rsrc_name);
                errorSession->AddError(
                    this,
                    ShaderToolsErrorSource::Reflection,
                    ShaderToolsErrorCode::ReflectionCouldNotStoreResource,
                    error_message.c_str());
            }
            else
            {
                iter->second.bindingIdx = descriptor_binding->binding;
                iter->second.setIdx = descriptor_binding->set;
            }

        }

        return ShaderToolsErrorCode::Success;
    }

    ShaderToolsErrorCode ShaderReflectorImpl::parsePushConstants(const ShaderStage stage)
    {
        uint32_t count = 0;
        SpvReflectResult result = spvReflectEnumeratePushConstantBlocks(spvReflectModule.get(), &count, nullptr);
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            errorSession->AddError(
                this,
                ShaderToolsErrorSource::Reflection,
                ShaderToolsErrorCode::SpvReflectErrorsStart,
                "Failed to enumerate push constant blocks");
            return ShaderToolsErrorCode::SpvReflectErrorsStart;
        }
        else if (count == 0)
        {
            return ShaderToolsErrorCode::Success;
        }

        std::vector<SpvReflectBlockVariable*> push_constants(count);
        result = spvReflectEnumeratePushConstantBlocks(spvReflectModule.get(), &count, push_constants.data());
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            errorSession->AddError(
                this,
                ShaderToolsErrorSource::Reflection,
                ShaderToolsErrorCode::SpvReflectErrorsStart,
                "Failed to enumerate push constant blocks");
            return ShaderToolsErrorCode::SpvReflectErrorsStart;
        }

        for (const SpvReflectBlockVariable* push_constant : push_constants)
        {
            PushConstantInfo push_constant_info;
            push_constant_info.SetStages(static_cast<VkShaderStageFlags>(stage.stageBits));
            
            if (!push_constant->name)
            {
                errorSession->AddError(
                    this,
                    ShaderToolsErrorSource::Reflection,
                    ShaderToolsErrorCode::SpvReflectErrorsStart,
                    "Push constant did not have a name!");
                return ShaderToolsErrorCode::SpvReflectErrorsStart;
            }

            push_constant_info.SetName(push_constant->name);

            std::vector<ShaderResourceSubObject> members = CreateSubobjects(push_constant->member_count, push_constant->members);
            push_constant_info.SetMembers(members.size(), members.data());

            if (push_constants.size() > 1)
            {

                uint32_t offset = 0;
                // we have to copy data we just set up earlier :(
                for (const auto& push_block : pushConstants)
                {
                    size_t num_members = 0;
                    push_block.second.GetMembers(&num_members, nullptr);
                    std::vector<ShaderResourceSubObject> members(num_members);
                    push_block.second.GetMembers(&num_members, members.data());
                    for (const auto& member : members)
                    {
                        offset += member.Size;
                    }
                }

                push_constant_info.SetOffset(offset);
            }

            pushConstants.emplace(static_cast<VkShaderStageFlags>(stage.stageBits), push_constant_info);

        }

        return ShaderToolsErrorCode::Success;
    }

    ShaderToolsErrorCode ShaderReflectorImpl::parseSpecializationConstants()
    {
        uint32_t count = 0;
        SpvReflectResult result = spvReflectEnumerateSpecializationConstants(spvReflectModule.get(), &count, nullptr);
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            errorSession->AddError(
                this,
                ShaderToolsErrorSource::Reflection,
                ShaderToolsErrorCode::SpvReflectErrorsStart,
                "Failed to enumerate specialization constants");
            return ShaderToolsErrorCode::SpvReflectErrorsStart;
        }
        else if (count == 0)
        {
            return ShaderToolsErrorCode::Success;
        }

        std::vector<SpvReflectSpecializationConstant*> specialization_constants(count);
        result = spvReflectEnumerateSpecializationConstants(spvReflectModule.get(), &count, specialization_constants.data());
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            errorSession->AddError(
                this,
                ShaderToolsErrorSource::Reflection,
                ShaderToolsErrorCode::SpvReflectErrorsStart,
                "Failed to enumerate specialization constants");
            return ShaderToolsErrorCode::SpvReflectErrorsStart;
        }

        for (const SpvReflectSpecializationConstant* spc : specialization_constants)
        {
            SpecializationConstant spc_result;
            spc_result.ConstantID = spc->constant_id;
            spc_result.SetName(spc->name);
            specializationConstants.emplace(spc->constant_id, spc_result);
        }

        return ShaderToolsErrorCode::Success;
    }

    ShaderToolsErrorCode ShaderReflectorImpl::parseBinary(const ShaderStage& shader_handle)
    {
        ReadRequest find_binary_request{ ReadRequest::Type::FindShaderBinaryForReflection, shader_handle };
        ReadRequestResult find_binary_result = MakeFileTrackerReadRequest(find_binary_request);
        if (!find_binary_result.has_value())
        {
            const std::string errorMessage = "Attempted to parse and generate bindings for binary that cannot be found";
            errorSession->AddError(
                this,
                ShaderToolsErrorSource::Reflection,
                find_binary_result.error(),
                errorMessage.c_str());
            return ShaderToolsErrorCode::ReflectionShaderBinaryNotFound;
        }
        std::vector<uint32_t> binary_vec = std::get<std::vector<uint32_t>>(*find_binary_result);
        return parseImpl(shader_handle, std::move(binary_vec));
    }

    void ShaderReflectorImpl::collateSets()
    {
        std::set<uint32_t> unique_keys;
        for (auto iter = tempResources.begin(); iter != tempResources.end(); ++iter)
        {
            unique_keys.insert(iter->first);
        }

        for (auto& unique_set : unique_keys)
        {
            if (sortedSets.count(unique_set) == 0)
            {
                sortedSets.emplace(unique_set, DescriptorSetInfo{ unique_set });
            }

            DescriptorSetInfo& curr_set = sortedSets.at(unique_set);
            auto obj_range = tempResources.equal_range(unique_set);
            for (auto iter = obj_range.first; iter != obj_range.second; ++iter)
            {
                const uint32_t& binding_idx = iter->second.BindingIdx();
                if (curr_set.Members.count(binding_idx) != 0)
                {
                    curr_set.Members.at(binding_idx).Stages() |= iter->second.UsedBy().stageBits;
                }
                else
                {
                    curr_set.Members.emplace(binding_idx, iter->second);
                }
            }
        }
    }

    ShaderToolsErrorCode ShaderReflectorImpl::parseImpl(const ShaderStage& shader_handle, std::vector<uint32_t> binary_data)
    {
        VkShaderStageFlags stage = static_cast<VkShaderStageFlags>(shader_handle.stageBits);

        SpvReflectShaderModule* module_ptr = new SpvReflectShaderModule();
        SpvReflectResult result = spvReflectCreateShaderModule(binary_data.size() * sizeof(uint32_t), binary_data.data(), module_ptr);
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            const std::string errorMessage = "Failed to create SpvReflectShaderModule from binary data";
            errorSession->AddError(
                this,
                ShaderToolsErrorSource::Reflection,
                ShaderToolsErrorCode::SpvReflectErrorsStart,
                errorMessage.c_str());
            return ShaderToolsErrorCode::SpvReflectErrorsStart;
        }

        // should be nullptr up to this point, only contains deleter
        spvReflectModule.reset(module_ptr);

        ShaderToolsErrorCode descriptorBindingsParseError = parseDescriptorBindings(shader_handle);
        if (descriptorBindingsParseError != ShaderToolsErrorCode::Success)
        {
            return descriptorBindingsParseError;
        }

        ShaderToolsErrorCode pushConstantsParseError = parsePushConstants(shader_handle);
        if (pushConstantsParseError != ShaderToolsErrorCode::Success)
        {
            return pushConstantsParseError;
        }

        ShaderToolsErrorCode specializationConstantsParseError = parseSpecializationConstants();
        if (specializationConstantsParseError != ShaderToolsErrorCode::Success)
        {
            return specializationConstantsParseError;
        }


        if (stage != VK_SHADER_STAGE_COMPUTE_BIT)
        {
            std::vector<VertexAttributeInfo> input_attributes = parseInterfaceVariables(InterfaceVariableType::Input);
            if (input_attributes.empty() && StageRequiresVertexInputs(stage))
            {
                return ShaderToolsErrorCode::ReflectionFailedToParseInputAttributes;
            }
            inputAttributes.emplace(stage, std::move(input_attributes));

            std::vector<VertexAttributeInfo> output_attributes = parseInterfaceVariables(InterfaceVariableType::Output);
            if (output_attributes.empty() && StageRequiresVertexOutputs(stage))
            {
                return ShaderToolsErrorCode::ReflectionFailedToParseOutputAttributes;
            }
            outputAttributes.emplace(stage, std::move(output_attributes));
        }

        collateSets();

        return ShaderToolsErrorCode::Success;
    }

    size_t ShaderReflectorImpl::getNumSets() const noexcept
    {
        return sortedSets.size();
    }

    std::string ShaderReflectorImpl::GetActualResourceName(const std::string& rsrc_name)
    {
        size_t first_idx = rsrc_name.find_first_of('_');
        std::string results;
        if (first_idx != std::string::npos)
        {
            results = std::string{ rsrc_name.cbegin() + first_idx + 1, rsrc_name.cend() };
        }
        else
        {
            results = rsrc_name;
        }

        size_t second_idx = results.find_last_of('_');
        if (second_idx != std::string::npos)
        {
            results.erase(results.begin() + second_idx, results.end());
        }

        return results;
    }

    void DestroySpvReflectShaderModule(SpvReflectShaderModule* module)
    {
        if (module != nullptr)
        {
            spvReflectDestroyShaderModule(module);
        }
    }

}
