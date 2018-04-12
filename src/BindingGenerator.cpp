#include "BindingGenerator.hpp"
#include "ShaderStructs.hpp"
#include "spirv-cross/spirv_cross.hpp"
#include "spirv-cross/spirv_glsl.hpp"
#include <array>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <map>
#include <set>
#define _SCL_SECURE_NO_WARNINGS

namespace st {

    extern std::unordered_map<Shader, std::string> shaderFiles;
    extern std::unordered_map<Shader, std::vector<uint32_t>> shaderBinaries;

    class BindingGeneratorImpl {
        BindingGeneratorImpl(const BindingGeneratorImpl&) = delete;
        BindingGeneratorImpl& operator=(const BindingGeneratorImpl&) = delete;
    public:

        BindingGeneratorImpl() = default;
        ~BindingGeneratorImpl() = default;
        BindingGeneratorImpl(BindingGeneratorImpl&& other) noexcept;
        BindingGeneratorImpl& operator=(BindingGeneratorImpl&& other) noexcept;

        void parseBinary(const std::vector<uint32_t>& binary_data, const VkShaderStageFlags stage);
        void collateSets(std::multimap<uint32_t, ShaderResource> sets);
        void parseBinary(const Shader& shader_handle);
        void parseImpl(const std::vector<uint32_t>& binary_data, const VkShaderStageFlags stage);

        std::unordered_multimap<VkShaderStageFlags, DescriptorSetInfo> descriptorSets;
        std::map<uint32_t, DescriptorSetInfo> sortedSets;
        std::unordered_map<VkShaderStageFlags, PushConstantInfo> pushConstants;
        std::unordered_map<VkShaderStageFlags, std::map<uint32_t, VertexAttributeInfo>> inputAttributes;
        std::unordered_map<VkShaderStageFlags, std::map<uint32_t, VertexAttributeInfo>> outputAttributes;
        std::map<uint32_t, SpecializationConstant> specializationConstants;

        decltype(sortedSets)::iterator findSetWithIdx(const uint32_t idx);
    };

    BindingGenerator::BindingGenerator() : impl(std::make_unique<BindingGeneratorImpl>()) {}

    BindingGenerator::~BindingGenerator() {}

    BindingGenerator::BindingGenerator(BindingGenerator&& other) noexcept : impl(std::move(other.impl)) {}

    BindingGeneratorImpl::BindingGeneratorImpl(BindingGeneratorImpl&& other) noexcept : descriptorSets(std::move(other.descriptorSets)),
        sortedSets(std::move(other.sortedSets)), pushConstants(std::move(other.pushConstants)) {}

    BindingGenerator& BindingGenerator::operator=(BindingGenerator&& other) noexcept {
        impl = std::move(other.impl);
        return *this;
    }

    BindingGeneratorImpl& BindingGeneratorImpl::operator=(BindingGeneratorImpl&& other) noexcept {
        descriptorSets = std::move(other.descriptorSets);
        sortedSets = std::move(other.sortedSets);
        pushConstants = std::move(other.pushConstants);
        return *this;
    }

    decltype(BindingGeneratorImpl::sortedSets)::iterator BindingGeneratorImpl::findSetWithIdx(const uint32_t idx) {
        return sortedSets.find(idx);
    }

    uint32_t BindingGenerator::GetNumSets() const noexcept {
        return static_cast<uint32_t>(impl->sortedSets.size());
    }

    std::map<uint32_t, ShaderResource> BindingGenerator::GetDescriptorSetObjects(const uint32_t & set_idx) const {
        return impl->sortedSets.at(set_idx).Members;
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

    void ParseResource(std::multimap<uint32_t, ShaderResource>& info, spirv_cross::Compiler& cmplr, const VkShaderStageFlags& stage, const VkDescriptorType& type_being_parsed, const std::vector<spirv_cross::Resource>& resources) {
        
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
        
        for (const auto& rsrc : resources) {
            ShaderResource obj;
            obj.Binding = cmplr.get_decoration(rsrc.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(rsrc.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(rsrc.id);
            if (parsing_buffer_type(type_being_parsed)) {
                obj.Members = extract_buffer_members(rsrc, cmplr);
            }
            obj.Type = type_being_parsed;
            auto spir_type = cmplr.get_type(rsrc.type_id);
            obj.StorageClass = StorageClassFromSPIRType(spir_type);
            obj.Format = FormatFromSPIRType(spir_type);
            obj.Stages = stage;
            info.emplace(obj.ParentSet, obj);
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

    void BindingGenerator::Clear() {
        impl.reset();
        impl = std::make_unique<BindingGeneratorImpl>();
    }

    void BindingGenerator::ParseBinary(const Shader & shader) {
        impl->parseBinary(shader);
    }
    
    void BindingGenerator::ParseBinary(const std::vector<uint32_t>& binary_data, const VkShaderStageFlags stage) {
        impl->parseBinary(binary_data, stage);
    }

    void BindingGeneratorImpl::parseBinary(const Shader& shader_handle) {
        if (shaderBinaries.count(shader_handle) == 0) {
            throw std::runtime_error("Attempted to parse binary that does not exist in current program!");
        }

        parseImpl(shaderBinaries.at(shader_handle), shader_handle.GetStage());
    }

    void BindingGeneratorImpl::parseBinary(const std::vector<uint32_t>& binary_data, const VkShaderStageFlags stage) {
        parseImpl(binary_data, stage);
    }

    void BindingGeneratorImpl::collateSets(std::multimap<uint32_t, ShaderResource> sets) {
        std::set<uint32_t> unique_keys;
        for (auto iter = sets.begin(); iter != sets.end(); ++iter) {
            unique_keys.insert(iter->first);
        }

        for (auto& unique_set : unique_keys) {
            if (sortedSets.count(unique_set) == 0) {
                sortedSets.emplace(unique_set, DescriptorSetInfo{ unique_set });
            }

            DescriptorSetInfo& curr_set = sortedSets.at(unique_set);
            auto obj_range = sets.equal_range(unique_set);
            for (auto iter = obj_range.first; iter != obj_range.second; ++iter) {
                const uint32_t& binding_idx = iter->second.Binding;
                if (curr_set.Members.count(binding_idx) != 0) {
                    curr_set.Members.at(binding_idx).Stages |= iter->second.Stages;
                }
                else {
                    curr_set.Members[binding_idx] = iter->second;
                }
            }
        }
    }

    void BindingGeneratorImpl::parseImpl(const std::vector<uint32_t>& binary_data, const VkShaderStageFlags stage) {
        using namespace spirv_cross;
        Compiler glsl(binary_data);

        std::multimap<uint32_t, ShaderResource> sets;
        const auto& resources = glsl.get_shader_resources();
        ParseResource(sets, glsl, stage, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, resources.uniform_buffers);
        ParseResource(sets, glsl, stage, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, resources.storage_buffers);
        ParseResource(sets, glsl, stage, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, resources.subpass_inputs);
        ParseResource(sets, glsl, stage, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, resources.separate_images);
        ParseResource(sets, glsl, stage, VK_DESCRIPTOR_TYPE_SAMPLER, resources.separate_samplers);
        ParseResource(sets, glsl, stage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, resources.sampled_images);
        ParseResource(sets, glsl, stage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, resources.storage_images);
        collateSets(std::move(sets));
        
        if (!resources.push_constant_buffers.empty()) {
            auto iter = pushConstants.emplace(stage, parsePushConstants(glsl, stage));
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

        inputAttributes.emplace(stage, parseInputAttributes(glsl));
        outputAttributes.emplace(stage, parseOutputAttributes(glsl));
    }

    std::map<std::string, VkDescriptorSetLayoutBinding> BindingGenerator::GetSetNameBindingPairs(const uint32_t & set_idx) const {
        auto iter = impl->sortedSets.find(set_idx);
        if (iter == impl->sortedSets.cend()) {
            return std::map<std::string, VkDescriptorSetLayoutBinding>();
        }

        const auto& set = iter->second;
        std::map<std::string, VkDescriptorSetLayoutBinding> results;
        for (auto& set : set.Members) {
            results.emplace(set.second.Name, (VkDescriptorSetLayoutBinding)set.second);
        }
        
        return results;
    }

    std::vector<VkPushConstantRange> BindingGenerator::GetPushConstantRanges() const {
        std::vector<VkPushConstantRange> ranges;
        for (auto& entry : impl->pushConstants) {
            ranges.push_back((VkPushConstantRange)entry.second);
        }
        return ranges;
    }

    std::vector<VkVertexInputAttributeDescription> BindingGenerator::GetVertexAttributes() const {
        if (impl->inputAttributes.count(VK_SHADER_STAGE_VERTEX_BIT) == 0) {
            return std::vector<VkVertexInputAttributeDescription>();
        }
        const auto& input_attrs = impl->inputAttributes.at(VK_SHADER_STAGE_VERTEX_BIT);
        
        std::vector<VkVertexInputAttributeDescription> actual_attrs;
        for (const auto& input_attr : input_attrs) {
            actual_attrs.emplace_back((VkVertexInputAttributeDescription)input_attr.second);
        }
        
        return actual_attrs;
    }

}