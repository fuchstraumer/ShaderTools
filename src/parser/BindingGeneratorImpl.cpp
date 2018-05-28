#include "BindingGeneratorImpl.hpp"
#include "easyloggingpp/src/easylogging++.h"
#include "../util/ShaderFileTracker.hpp"
#include "core/ShaderResource.hpp"
#include "core/ResourceUsage.hpp"
#include <array>
#ifdef FindResource
#undef FindResource
#endif // FindResource
#include "../lua/ResourceFile.hpp"

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
                LOG(ERROR) << "SPIRType somehow has invalid access qualifier enum value!";
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

    void BindingGeneratorImpl::parseResourceType(const Shader& shader_handle, const VkDescriptorType& type_being_parsed) {
        
        auto& f_tracker = ShaderFileTracker::GetFileTracker();
        const auto& rsrc_path = f_tracker.ShaderUsedResourceScript.at(shader_handle);
        const auto* rsrc_script = f_tracker.ResourceScripts.at(rsrc_path).get();

        auto get_actual_name = [](const std::string& rsrc_name)->std::string {
            size_t first_idx = rsrc_name.find_first_of('_');
            std::string results;
            if (first_idx != std::string::npos) {
                results = std::string{ rsrc_name.cbegin() + first_idx + 1, rsrc_name.cend() };
            }
            else {
                results = rsrc_name;
            }
            
            size_t second_idx = results.find_last_of('_');
            if (second_idx != std::string::npos) {
                results.erase(results.begin() + second_idx, results.end());
            }

            return results;
        };
     
        auto parsing_buffer_type = [](const VkDescriptorType& type) {
            return (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) || (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
                (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) || (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
        };

        auto extract_buffer_members = [](const spirv_cross::Resource& rsrc, const spirv_cross::Compiler& cmplr)->std::vector<ShaderResourceSubObject> {
            std::vector<ShaderResourceSubObject> results;
            auto ranges = cmplr.get_active_buffer_ranges(rsrc.id);
            for (auto& range : ranges) {
                ShaderResourceSubObject member;
                member.Name = strdup(cmplr.get_member_name(rsrc.base_type_id, range.index).c_str());
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
            LOG(ERROR) << "Passed invalid resource type during binding generation";
            throw std::runtime_error("Passed invalid resource type.");
        };

        for (const auto& rsrc : resources) {
            const std::string rsrc_name = get_actual_name(rsrc.name);
            const ShaderResource* parent_resource = rsrc_script->FindResource(rsrc_name);
            if (parent_resource == nullptr) {
                LOG(ERROR) << "Couldn't find parent resource of resource usage object!";
                throw std::runtime_error("Couldn't find parent resource for resource usage object.");
            }
            uint32_t binding_idx = recompiler->get_decoration(rsrc.id, spv::DecorationBinding);
            uint32_t set_idx = recompiler->get_decoration(rsrc.id, spv::DecorationDescriptorSet);
            auto spir_type = recompiler->get_type(rsrc.type_id);
            access_modifier modifier = AccessModifierFromSPIRType(spir_type);
            tempResources.emplace(set_idx, ResourceUsage(shader_handle, parent_resource, binding_idx, modifier, parent_resource->GetType()));
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
            member.Name = strdup(cmplr.get_member_name(pconstant.base_type_id, range.index).c_str());
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
            LOG(ERROR) << "Attempted to parse and generate bindings for binary that cannot be found!";
            throw std::runtime_error("Attempted to parse binary that does not exist in current program!");
        }

        parseImpl(shader_handle, binary_vec);
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
                    curr_set.Members.at(binding_idx).Stages() |= iter->second.UsedBy().GetStage();
                }
                else {
                    curr_set.Members.emplace(binding_idx, iter->second);
                }
            }
        }
    }

    void BindingGeneratorImpl::parseSpecializationConstants() {
        using namespace spirv_cross;
        auto constants = recompiler->get_specialization_constants();
        if (!constants.empty()) {
            for (const auto& spc : constants) {
                SpecializationConstant spc_new;
                spc_new.ConstantID = spc.constant_id;
                if (specializationConstants.count(spc.constant_id) != 0) {
                    // Already registered this one.
                    continue;
                }
                const SPIRConstant& spc_value = recompiler->get_constant(spc.id);
                const SPIRType& spc_type = recompiler->get_type(spc_value.constant_type);
                

                if (spc_type.columns > 1 || spc_type.vecsize > 1) {
                    LOG(ERROR) << "Matrix/vector specialization constants are currently not supported!";
                    throw std::domain_error("Attempted to use unsupported specialization constant type.");
                }

                switch (spc_type.basetype) {
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
                case SPIRType::Int64:
                    spc_new.Type = SpecializationConstant::constant_type::i64;
                    spc_new.value_i64 = spc_value.scalar_i64();
                    break;
                }

                specializationConstants.emplace(spc.constant_id, spc_new);
            }
        }
    }

    void BindingGeneratorImpl::parseImpl(const Shader& handle, const std::vector<uint32_t>& binary_data) {
        using namespace spirv_cross;
        recompiler = std::make_unique<Compiler>(binary_data);
        const auto stage = handle.GetStage();
        parseResourceType(handle, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        parseResourceType(handle, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        parseResourceType(handle, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
        parseResourceType(handle, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        parseResourceType(handle, VK_DESCRIPTOR_TYPE_SAMPLER);
        parseResourceType(handle, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        parseResourceType(handle, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        collateSets();
        parseSpecializationConstants();

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
