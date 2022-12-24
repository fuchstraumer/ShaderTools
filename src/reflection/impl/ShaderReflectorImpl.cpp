#include "ShaderReflectorImpl.hpp"
#include "../../util/ShaderFileTracker.hpp"
#include "../../parser/yamlFile.hpp"
#include "resources/ShaderResource.hpp"
#include "resources/ResourceUsage.hpp"
#include "spirv_cross_containers.hpp"
#include <array>
#include <iostream>
#include <fstream>

namespace st
{
    constexpr bool IsBufferType(const VkDescriptorType& type) noexcept
    {
        return (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) || (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
            (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) || (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
    }

    ShaderReflectorImpl::ShaderReflectorImpl(yamlFile* yaml_file) : rsrcFile(yaml_file) {}

    ShaderReflectorImpl::ShaderReflectorImpl(ShaderReflectorImpl&& other) noexcept : descriptorSets(std::move(other.descriptorSets)),
        sortedSets(std::move(other.sortedSets)), pushConstants(std::move(other.pushConstants)), rsrcFile{ nullptr } {}

    ShaderReflectorImpl& ShaderReflectorImpl::operator=(ShaderReflectorImpl&& other) noexcept
    {
        descriptorSets = std::move(other.descriptorSets);
        sortedSets = std::move(other.sortedSets);
        pushConstants = std::move(other.pushConstants);
        return *this;
    }

    std::vector<VertexAttributeInfo> parseVertAttrs(const spirv_cross::Compiler& cmplr, const spirv_cross::SmallVector<spirv_cross::Resource>& rsrcs)
    {
        std::vector<VertexAttributeInfo> attributes;
        uint32_t idx = 0;
        uint32_t running_offset = 0;

        for (const auto& attr : rsrcs)
        {
            VertexAttributeInfo attr_info;
            attr_info.SetName(cmplr.get_name(attr.id).c_str());
            attr_info.SetLocation(cmplr.get_decoration(attr.id, spv::DecorationLocation));
            attr_info.SetOffset(running_offset);
            const spirv_cross::SPIRType attr_type = cmplr.get_type(attr.type_id);
            attr_info.SetType(&attr_type);
            running_offset += attr_type.vecsize * attr_type.width;
            attributes.emplace_back(std::move(attr_info));
            ++idx;
        }

        return attributes;
    }

    std::vector<VertexAttributeInfo> parseInputAttributes(const spirv_cross::Compiler& cmplr)
    {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        return parseVertAttrs(cmplr, rsrcs.stage_inputs);
    }

    std::vector<VertexAttributeInfo> parseOutputAttributes(const spirv_cross::Compiler& cmplr)
    {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        return parseVertAttrs(cmplr, rsrcs.stage_outputs);
    }

    void ShaderReflectorImpl::parseResourceType(const ShaderStage& shader_handle, const VkDescriptorType& type_being_parsed)
    {
        auto& f_tracker = ShaderFileTracker::GetFileTracker();

        auto get_actual_name = [](const std::string& rsrc_name)->std::string
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
        };

        auto extract_buffer_members = [](const spirv_cross::Resource& rsrc, const spirv_cross::Compiler& cmplr)->std::vector<ShaderResourceSubObject>
        {
            std::vector<ShaderResourceSubObject> results;
            auto ranges = cmplr.get_active_buffer_ranges(rsrc.id);
            for (auto& range : ranges)
            {
                ShaderResourceSubObject member;
                member.Name = strdup(cmplr.get_member_name(rsrc.base_type_id, range.index).c_str());
                member.Size = static_cast<uint32_t>(range.range);
                member.Offset = static_cast<uint32_t>(range.offset);
                results.emplace_back(member);
            }
            return results;
        };

        const auto resources_all = recompiler->get_shader_resources();
        spirv_cross::SmallVector<spirv_cross::Resource> resources;
        switch (type_being_parsed)
        {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            resources = resources_all.uniform_buffers;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            resources = resources_all.storage_buffers;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            resources = resources_all.storage_images;
            break;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            resources = resources_all.sampled_images;
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            resources = resources_all.separate_samplers;
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            resources = resources_all.separate_images;
            break;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            resources = resources_all.subpass_inputs;
            break;
        default:
            throw std::runtime_error("Passed invalid resource type during binding generation.");
        };

        const std::string shader_name = f_tracker.GetShaderName(shader_handle);

        for (const auto& rsrc : resources)
        {
            const std::string rsrc_name = get_actual_name(recompiler->get_name(rsrc.id));
            const ShaderResource* parent_resource = rsrcFile->FindResource(rsrc_name);
            const std::string parent_group_name = parent_resource->ParentGroupName();

            if (usedResourceGroupNames.count(parent_group_name) == 0)
            {
                usedResourceGroupNames.emplace(parent_group_name);
            }

            glsl_qualifier curr_qualifier = parent_resource->GetReadWriteQualifierForShader(shader_name.c_str());
            if (parent_resource == nullptr)
            {
                throw std::runtime_error("Couldn't find parent resource for resource usage object.");
            }

            uint32_t binding_idx = recompiler->get_decoration(rsrc.id, spv::DecorationBinding);
            if (binding_idx != parent_resource->BindingIndex())
            {
                throw std::runtime_error("Binding index of generated shader code, and binding of the actual resource, did not match!");
            }

            // If following logic fails, we just use read-write as it's a perfectly fine fallback
            access_modifier modifier(access_modifier::ReadWrite);
            if (type_being_parsed == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type_being_parsed == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            {
                modifier = access_modifier::Read;
            }
            else if (recompiler->has_decoration(rsrc.id, spv::DecorationNonWritable))
            {
                modifier = access_modifier::Read;
            }
            else if (recompiler->has_decoration(rsrc.id, spv::DecorationNonReadable))
            {
                modifier = access_modifier::Write;
            }
            else if (curr_qualifier != glsl_qualifier::InvalidQualifier)
            {
                if (curr_qualifier == glsl_qualifier::ReadOnly)
                {
                    modifier = access_modifier::Read;
                }
                else
                {
                    // if we get this far, the only other option is writeonly
                    modifier = access_modifier::Write;
                }
            }
            const uint32_t set_idx = recompiler->get_decoration(rsrc.id, spv::DecorationDescriptorSet);

            if (resourceGroupSetIndices.count(parent_group_name) == 0)
            {
                resourceGroupSetIndices.emplace(parent_group_name, set_idx);
            }

            auto iter = tempResources.emplace(set_idx,
                ResourceUsage(shader_handle, parent_resource, modifier, parent_resource->DescriptorType()));
            iter->second.bindingIdx = binding_idx;
            iter->second.setIdx = set_idx;
        }

    }

    PushConstantInfo parsePushConstants(const spirv_cross::Compiler& cmplr, const VkShaderStageFlags& stage)
    {
        const auto push_constants = cmplr.get_shader_resources();
        const auto& pconstant = push_constants.push_constant_buffers.front();

        std::vector<spirv_cross::BufferRange> ranges;
        {
            auto ranges_sv = cmplr.get_active_buffer_ranges(pconstant.id);
            std::copy(std::begin(ranges_sv), std::end(ranges_sv), std::back_inserter(ranges));
        }

        PushConstantInfo result;
        result.SetStages(stage);
        result.SetName(cmplr.get_name(pconstant.id).c_str());
        std::vector<ShaderResourceSubObject> members;

        auto sort_buffer_range = [](const spirv_cross::BufferRange& br0, const spirv_cross::BufferRange& br1)
        {
            return br0.offset < br1.offset;
        };

        std::sort(std::begin(ranges), std::end(ranges), sort_buffer_range);

        for(auto& range : ranges)
        {
            ShaderResourceSubObject member;
            member.Name = strdup(cmplr.get_member_name(pconstant.base_type_id, range.index).c_str());
            member.Size = static_cast<uint32_t>(range.range);
            member.Offset = static_cast<uint32_t>(range.offset);
            members.emplace_back(std::move(member));
        }

        result.SetMembers(members.size(), members.data());

        return std::move(result);
    }

    void ShaderReflectorImpl::parseBinary(const ShaderStage& shader_handle)
    {
        auto& FileTracker = ShaderFileTracker::GetFileTracker();

        std::vector<uint32_t> binary_vec;
        if (!FileTracker.FindShaderBinary(shader_handle, binary_vec))
        {
            throw std::runtime_error("Attempted to parse and generate bindings for binary that cannot be found");
        }

        parseImpl(shader_handle, binary_vec);
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

    void ShaderReflectorImpl::parseSpecializationConstants()
    {
        using namespace spirv_cross;

        SmallVector<spirv_cross::SpecializationConstant> constants = recompiler->get_specialization_constants();
        if (!constants.empty())
        {
            for (const auto& spc : constants)
            {
                SpecializationConstant spc_new;
                spc_new.ConstantID = spc.constant_id;
                if (specializationConstants.count(spc.constant_id) != 0)
                {
                    // Already registered this one.
                    continue;
                }

                const SPIRConstant & spc_value = recompiler->get_constant(spc.id);
                const SPIRType& spc_type = recompiler->get_type(spc_value.constant_type);

                if (spc_type.columns > 1 || spc_type.vecsize > 1)
                {
                    throw std::runtime_error("Attempted to use a vector or matrix specialization constant, which is not possible");
                }

                switch (spc_type.basetype)
                {
                case SPIRType::Boolean:
                    spc_new.Type = SpecializationConstant::constant_type::b32;
                    spc_new.value_b32 = static_cast<VkBool32>(spc_value.scalar());
                    break;
                case SPIRType::UInt:
                    spc_new.Type = SpecializationConstant::constant_type::ui32;
                    spc_new.value_ui32 = spc_value.scalar();
                    break;
                case SPIRType::Int:
                    spc_new.Type = SpecializationConstant::constant_type::i32;
                    spc_new.value_i32 = spc_value.scalar_i32();
                    break;
                case SPIRType::Float:
                    spc_new.Type = SpecializationConstant::constant_type::f32;
                    spc_new.value_f32 = spc_value.scalar_f32();
                    break;
                case SPIRType::Double:
                    spc_new.Type = SpecializationConstant::constant_type::f64;
                    spc_new.value_f64 = spc_value.scalar_f64();
                    break;
                case SPIRType::UInt64:
                    spc_new.Type = SpecializationConstant::constant_type::ui32;
                    spc_new.value_ui64 = spc_value.scalar_u64();
                    break;
                case SPIRType::Int64:
                    spc_new.Type = SpecializationConstant::constant_type::i64;
                    spc_new.value_i64 = spc_value.scalar_i64();
                    break;
                default:
                    throw std::runtime_error("Encountered unsupported specialization constant type during parsing of specialization constants. Update enum.");
                }

                specializationConstants.emplace(spc.constant_id, spc_new);
            }
        }
    }

    void ShaderReflectorImpl::parseImpl(const ShaderStage& handle, const std::vector<uint32_t>& binary_data)
    {
        using namespace spirv_cross;

        recompiler = std::make_unique<CompilerGLSL>(binary_data);
        CompilerGLSL::Options options;
        options.vulkan_semantics = true;
        // we set this ourselves in most locations!
        options.enable_storage_image_qualifier_deduction = false;
        recompiler->set_common_options(options);
        std::string recompiled_source;

        try
        {
            recompiled_source = recompiler->compile();
        }
        catch (const spirv_cross::CompilerError& e)
        {
            std::cerr << "Failed to fully parse/recompile SPIR-V binary back to GLSL text. Outputting partial source thus far.";
            std::cerr << "spirv_cross::CompilerError.what(): " << e.what() << "\n";

            recompiled_source = recompiler->get_partial_source();

            auto& sft = ShaderFileTracker::GetFileTracker();
            const std::string output_name = sft.GetShaderName(handle) + std::string{ "_failed_recompile.glsl" };
            std::ofstream output_stream(output_name);
            output_stream << recompiled_source;
            output_stream.flush();
            output_stream.close();

            throw e;
        }

        auto& sft = ShaderFileTracker::GetFileTracker();
        auto iter = sft.RecompiledSourcesFromBinaries.emplace(handle, recompiled_source);

        constexpr static VkDescriptorType supportedDescriptorTypes[]
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLER,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
        };

        for (size_t i = 0; i < std::size(supportedDescriptorTypes); ++i)
        {
            parseResourceType(handle, supportedDescriptorTypes[i]);
        }

        collateSets();
        parseSpecializationConstants();

        spirv_cross::ShaderResources resources = recompiler->get_shader_resources();
        const VkShaderStageFlagBits stage = static_cast<VkShaderStageFlagBits>(handle.stageBits);
        if (!resources.push_constant_buffers.empty())
        {
            auto iter = pushConstants.emplace(stage, parsePushConstants(*recompiler, stage));

            if (pushConstants.size() > 1)
            {
                uint32_t offset = 0;

                for (const auto& push_block : pushConstants)
                {
                    size_t num_members = 0;
                    push_block.second.GetMembers(&num_members, nullptr);
                    std::vector<ShaderResourceSubObject> members(num_members);
                    push_block.second.GetMembers(&num_members, members.data());

                    for (const auto& push_member : members)
                    {
                        offset += push_member.Size;
                    }

                }

                iter.first->second.SetOffset(offset);
            }

        }

        inputAttributes.emplace(stage, parseInputAttributes(*recompiler));
        outputAttributes.emplace(stage, parseOutputAttributes(*recompiler));
        // is this needed? is there maybe a more efficient way of doing this? reallocating seems silly...
        // (potentially not, creation takes in the binary data blob)
        recompiler.reset();
    }

    size_t ShaderReflectorImpl::getNumSets() const noexcept
    {
        return sortedSets.size();
    }

}
