#include "BindingGeneratorImpl.hpp"
#include "../util/ShaderFileTracker.hpp"
#include "core/ShaderResource.hpp"
#include "core/ResourceUsage.hpp"
#include <array>

namespace st {

    access_modifier AccessModifierFromSPIRType(const spirv_cross::SPIRType & type) {
        using namespace spirv_cross;
        if (type.basetype == SPIRType::SampledImage || type.basetype == SPIRType::Sampler || type.basetype == SPIRType::Struct || type.basetype == SPIRType::AtomicCounter) {
            return access_modifier::Read;
        }
        else {
            switch (type.image.access) {
            case spv::AccessQualifier::AccessQualifierReadOnly:
                return access_modifier::Read;
            case spv::AccessQualifier::AccessQualifierWriteOnly:
                return access_modifier::Write;
            case spv::AccessQualifier::AccessQualifierReadWrite:
                return access_modifier::ReadWrite;
            case spv::AccessQualifier::AccessQualifierMax:
                // Usually happens for storage images.
                return access_modifier::ReadWrite;
            default:
                throw std::domain_error("SPIRType somehow has invalid access qualifier enum value!");
            }
        }
    }

    BindingGeneratorImpl::BindingGeneratorImpl(BindingGeneratorImpl&& other) noexcept : descriptorSets(std::move(other.descriptorSets)),
        sortedSets(std::move(other.sortedSets)), pushConstants(std::move(other.pushConstants)) {}

    BindingGeneratorImpl& BindingGeneratorImpl::operator=(BindingGeneratorImpl&& other) noexcept {
        descriptorSets = std::move(other.descriptorSets);
        sortedSets = std::move(other.sortedSets);
        pushConstants = std::move(other.pushConstants);
        return *this;
    }

    size_t BindingGeneratorImpl::getNumSets() const noexcept {
        return sortedSets.size();
    }

    constexpr static std::array<VkShaderStageFlags, 6> possible_stages{
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_GEOMETRY_BIT,
        VK_SHADER_STAGE_COMPUTE_BIT,
        VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
    };

    std::map<uint32_t, VertexAttributeInfo> parseVertAttrs(const spirv_cross::Compiler& cmplr, const std::vector<spirv_cross::Resource>& rsrcs) {
        std::map<uint32_t, VertexAttributeInfo> attributes;
        uint32_t idx = 0;
        uint32_t running_offset = 0;
        for (const auto& attr : rsrcs) {
            VertexAttributeInfo attr_info;
            attr_info.Name = cmplr.get_name(attr.id);
            attr_info.Location = cmplr.get_decoration(attr.id, spv::DecorationLocation);
            attr_info.Offset = running_offset;
            attr_info.Type = cmplr.get_type(attr.type_id);
            running_offset += attr_info.Type.vecsize * attr_info.Type.width;
            attributes.emplace(attr_info.Location, std::move(attr_info));
            ++idx;
        }
        return attributes;
    }

    std::map<uint32_t, VertexAttributeInfo> parseInputAttributes(const spirv_cross::Compiler& cmplr) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        return parseVertAttrs(cmplr, rsrcs.stage_inputs);
    }

    std::map<uint32_t, VertexAttributeInfo> parseOutputAttributes(const spirv_cross::Compiler& cmplr) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        return parseVertAttrs(cmplr, rsrcs.stage_outputs);
    }

    void BindingGeneratorImpl::parseResourceType(const VkShaderStageFlags& stage, const VkDescriptorType& type_being_parsed) {
        
        auto& f_tracker = ShaderFileTracker::GetFileTracker();
     
        auto parsing_buffer_type = [](const VkDescriptorType& type) {
            return (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) || (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
                (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) || (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
        };

        auto extract_buffer_members = [](const spirv_cross::Resource& rsrc, const spirv_cross::Compiler& cmplr)->std::vector<ShaderResourceSubObject> {
            std::vector<ShaderResourceSubObject> results;
            auto ranges = cmplr.get_active_buffer_ranges(rsrc.id);
            for (auto& range : ranges) {
                ShaderResourceSubObject member;
                member.Name = cmplr.get_member_name(rsrc.base_type_id, range.index);
                member.Size = static_cast<uint32_t>(range.range);
                member.Offset = static_cast<uint32_t>(range.offset);
                results.emplace_back(member);
            }
            return results;
        };
        
        const auto resources_all = recompiler->get_shader_resources();
        std::vector<spirv_cross::Resource> resources;
        switch (type_being_parsed) {
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
            throw std::runtime_error("Passed invalid resource type.");
        };

        for (const auto& rsrc : resources) {
            uint32_t binding_idx = recompiler->get_decoration(rsrc.id, spv::DecorationBinding);
            uint32_t set_idx = recompiler->get_decoration(rsrc.id, spv::DecorationDescriptorSet);
            auto spir_type = recompiler->get_type(rsrc.type_id);
            access_modifier modifier = AccessModifierFromSPIRType(spir_type);
               
        }

    }

    PushConstantInfo parsePushConstants(const spirv_cross::Compiler& cmplr, const VkShaderStageFlags& stage) {
        const auto push_constants = cmplr.get_shader_resources();
        const auto& pconstant = push_constants.push_constant_buffers.front();
        auto ranges = cmplr.get_active_buffer_ranges(pconstant.id);
        PushConstantInfo result;
        result.Stages = stage;
        result.Name = cmplr.get_name(pconstant.id);
        for(auto& range : ranges) {
            ShaderResourceSubObject member;
            member.Name = cmplr.get_member_name(pconstant.base_type_id, range.index);
            member.Size = static_cast<uint32_t>(range.range);
            member.Offset = static_cast<uint32_t>(range.offset);
            result.Members.push_back(std::move(member));
        }
        return result;
    }

    void BindingGeneratorImpl::parseBinary(const Shader& shader_handle) {
        auto& FileTracker = ShaderFileTracker::GetFileTracker();
        std::vector<uint32_t> binary_vec;
        if (!FileTracker.FindShaderBinary(shader_handle, binary_vec)) {
            throw std::runtime_error("Attempted to parse binary that does not exist in current program!");
        }

        parseImpl(binary_vec, shader_handle.GetStage());
    }

    void BindingGeneratorImpl::parseBinary(const std::vector<uint32_t>& binary_data, const VkShaderStageFlags stage) {
        parseImpl(binary_data, stage);
    }

    void BindingGeneratorImpl::collateSets() {
        std::set<uint32_t> unique_keys;
        for (auto iter = tempResources.begin(); iter != tempResources.end(); ++iter) {
            unique_keys.insert(iter->first);
        }

        for (auto& unique_set : unique_keys) {
            if (sortedSets.count(unique_set) == 0) {
                sortedSets.emplace(unique_set, DescriptorSetInfo{ unique_set });
            }

            DescriptorSetInfo& curr_set = sortedSets.at(unique_set);
            auto obj_range = tempResources.equal_range(unique_set);
            for (auto iter = obj_range.first; iter != obj_range.second; ++iter) {
                const uint32_t& binding_idx = iter->second.BindingIdx();
                if (curr_set.Members.count(binding_idx) != 0) {
                    curr_set.Members.at(binding_idx)->Stages() |= iter->second.UsedBy().GetStage();
                }
                else {
                    *curr_set.Members[binding_idx] = iter->second;
                }
            }
        }
    }

    void BindingGeneratorImpl::parseImpl(const std::vector<uint32_t>& binary_data, const VkShaderStageFlags stage) {
        using namespace spirv_cross;
        recompiler = std::make_unique<Compiler>(binary_data);

        parseResourceType(stage, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        parseResourceType(stage, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        parseResourceType(stage, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
        parseResourceType(stage, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        parseResourceType(stage, VK_DESCRIPTOR_TYPE_SAMPLER);
        parseResourceType(stage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        parseResourceType(stage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        collateSets();
        
        auto resources = recompiler->get_shader_resources();
        if (!resources.push_constant_buffers.empty()) {
            auto iter = pushConstants.emplace(stage, parsePushConstants(*recompiler, stage));
            if (pushConstants.size() > 1) {
                uint32_t offset = 0;
                for (const auto& push_block : pushConstants) {
                    for (const auto& push_member : push_block.second.Members) {
                        offset += push_member.Size;
                    }
                }
                iter.first->second.Offset = offset;
            }
        }

        inputAttributes.emplace(stage, parseInputAttributes(*recompiler));
        outputAttributes.emplace(stage, parseOutputAttributes(*recompiler));
        recompiler.reset();
    }
    
}