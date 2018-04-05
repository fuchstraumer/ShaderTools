#include "BindingGenerator.hpp"
#include "ShaderStructs.hpp"
#include "spirv-cross/spirv_cross.hpp"
#include "spirv-cross/spirv_glsl.hpp"
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

        void parseBinary(const uint32_t binary_sz, const uint32_t* binary, const VkShaderStageFlags stage);
        void collateSets(std::multimap<uint32_t, DescriptorObject> sets);
        void parseBinary(const Shader& shader_handle);
        void parseImpl(const std::vector<uint32_t>& binary_data, const VkShaderStageFlags stage);

        std::unordered_multimap<VkShaderStageFlags, DescriptorSetInfo> descriptorSets;
        std::map<uint32_t, DescriptorSetInfo> sortedSets;
        std::unordered_map<VkShaderStageFlags, PushConstantInfo> pushConstants;
        std::unordered_map<VkShaderStageFlags, std::map<uint32_t, VertexAttributeInfo>> inputAttributes;
        std::unordered_map<VkShaderStageFlags, std::map<uint32_t, VertexAttributeInfo>> outputAttributes;

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
    
    void parseUniformBuffers(std::multimap<uint32_t, DescriptorObject>& info, const spirv_cross::Compiler& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.uniform_buffers) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            auto ranges = cmplr.get_active_buffer_ranges(ubuff.id);
            for(auto& range : ranges) {
                ShaderDataObject member;
                member.Name = cmplr.get_member_name(ubuff.base_type_id, range.index);
                member.Size = static_cast<uint32_t>(range.range);
                member.Offset = static_cast<uint32_t>(range.offset);
                obj.Members.push_back(std::move(member));
            }
            obj.Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            info.emplace(obj.ParentSet, obj);
        }
    }

    void parseStorageBuffers(std::multimap<uint32_t, DescriptorObject>& info, const spirv_cross::Compiler& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& sbuff : rsrcs.storage_buffers) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(sbuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(sbuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(sbuff.id);
            auto ranges = cmplr.get_active_buffer_ranges(sbuff.id);
            for (const auto& member : ranges) {
                ShaderDataObject sdo;
                sdo.Name = cmplr.get_member_name(sbuff.base_type_id, member.index);
                sdo.Size = static_cast<uint32_t>(member.range);
                sdo.Offset = static_cast<uint32_t>(member.offset);
                obj.Members.push_back(sdo);
            }
            obj.Type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            info.emplace(obj.ParentSet, std::move(obj));
        }
    }

    void parseInputAttachments(std::multimap<uint32_t, DescriptorObject>& info, const spirv_cross::Compiler& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.subpass_inputs) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            obj.Type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            info.emplace(obj.ParentSet, std::move(obj));
        }
    }

    void parseStorageImages(std::multimap<uint32_t, DescriptorObject>& info, const spirv_cross::Compiler& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.storage_images) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            obj.Type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            info.emplace(obj.ParentSet, std::move(obj));
        }
    }

    void parseCombinedSampledImages(std::multimap<uint32_t, DescriptorObject>& info, const spirv_cross::Compiler& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.sampled_images) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            obj.Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            info.emplace(obj.ParentSet, std::move(obj));
        }
    }

    void parseSeparableSampledImages(std::multimap<uint32_t, DescriptorObject>& info, const spirv_cross::Compiler& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.separate_images) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            obj.Type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            info.emplace(obj.ParentSet, std::move(obj));
        }
    }

    void parseSeparableSamplers(std::multimap<uint32_t, DescriptorObject>& info, const spirv_cross::Compiler& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.separate_samplers) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            obj.Type = VK_DESCRIPTOR_TYPE_SAMPLER;
            info.emplace(obj.ParentSet, std::move(obj));
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
            ShaderDataObject member;
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
    
    void BindingGenerator::ParseBinary(const uint32_t binary_size, const uint32_t* binary, const VkShaderStageFlags stage) {
        impl->parseBinary(binary_size, binary, stage);
    }

    void BindingGeneratorImpl::parseBinary(const Shader& shader_handle) {
        if (shaderBinaries.count(shader_handle) == 0) {
            throw std::runtime_error("Attempted to parse binary that does not exist in current program!");
        }

        parseImpl(shaderBinaries.at(shader_handle), shader_handle.GetStage());
    }

    void BindingGeneratorImpl::parseBinary(const uint32_t sz, const uint32_t* src, const VkShaderStageFlags stage) {
        std::vector<uint32_t> binary{ src, src + sz };
        parseImpl(binary, stage);
    }

    void BindingGeneratorImpl::collateSets(std::multimap<uint32_t, DescriptorObject> sets) {
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

        std::multimap<uint32_t, DescriptorObject> sets;
        parseUniformBuffers(sets, glsl, stage);
        parseStorageBuffers(sets, glsl, stage);
        parseInputAttachments(sets, glsl, stage);
        parseStorageImages(sets, glsl, stage);
        parseCombinedSampledImages(sets, glsl, stage);
        parseSeparableSampledImages(sets, glsl, stage);
        parseSeparableSamplers(sets, glsl, stage);
        collateSets(std::move(sets));
        
        const auto& rsrcs = glsl.get_shader_resources();
        if (!rsrcs.push_constant_buffers.empty()) {
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

    void BindingGenerator::GetLayoutBindings(const uint32_t& idx, uint32_t* num_bindings, VkDescriptorSetLayoutBinding* bindings) const {

        auto iter = impl->sortedSets.find(idx);
        if (iter == impl->sortedSets.cend()) {
            *num_bindings = 0;
        }

        const auto& set = iter->second;

        *num_bindings = static_cast<uint32_t>(set.Members.size());
        if (bindings != nullptr) {
            std::vector<VkDescriptorSetLayoutBinding> result;
            for (auto& member : set.Members) {
                result.push_back((VkDescriptorSetLayoutBinding)member.second);
            }
            std::copy(result.cbegin(), result.cend(), bindings);
        }
    }

    void BindingGenerator::GetSetMemberNames(const uint32_t& set_idx, uint32_t* num_names, char** names) const {
        
        auto iter = impl->sortedSets.find(set_idx);
        if (iter == impl->sortedSets.cend()) {
            *num_names = 0;
            return;
        }

        const auto& set = iter->second;
        *num_names = static_cast<uint32_t>(set.Members.size());
        if (names != nullptr) {
            std::vector<std::string> member_names;
            for (const auto& member : set.Members) {
                member_names.emplace_back(member.second.Name);
            }
            std::copy(member_names.cbegin(), member_names.cend(), names);
        }

    }

    void BindingGenerator::GetPushConstantRanges(uint32_t * num_ranges, VkPushConstantRange * results) const {
        std::vector<VkPushConstantRange> ranges;
        for (auto& entry : impl->pushConstants) {
            ranges.push_back((VkPushConstantRange)entry.second);
        }
        *num_ranges = static_cast<uint32_t>(ranges.size());
        if (results != nullptr) {
            std::copy(ranges.cbegin(), ranges.cend(), results);
        }
    }

    void BindingGenerator::GetVertexAttributes(uint32_t * num_attrs, VkVertexInputAttributeDescription * attrs) const {
        const auto& input_attrs = impl->inputAttributes.at(VK_SHADER_STAGE_VERTEX_BIT);
        *num_attrs = static_cast<uint32_t>(input_attrs.size());
        if (attrs != nullptr) {
            std::vector<VkVertexInputAttributeDescription> actual_attrs;
            for (const auto& input_attr : input_attrs) {
                actual_attrs.emplace_back((VkVertexInputAttributeDescription)input_attr.second);
            }

            std::copy(actual_attrs.cbegin(), actual_attrs.cend(), attrs);
        }
    }

    void BindingGenerator::SaveToJSON(const char* output_name) {

        namespace fs = std::experimental::filesystem;
        fs::path output_path(output_name);
        if (!output_path.has_extension()) {
            output_path.replace_extension(".json");
        }
        else if (output_path.extension().string() != std::string(".json")) {
            output_path.replace_extension(".json");
        }

        std::ofstream output_file(output_path.string());
        if (!output_file.is_open()) {
            throw std::runtime_error("Couldn't open JSON output file!");
        }

        nlohmann::json out;
        output_file << std::setw(4);
        out = impl->sortedSets;

        std::vector<StageAttributes> collected_attr;

        for (size_t i = 0; i < possible_stages.size(); ++i) {
            StageAttributes stage_attr;
            stage_attr.Stage = possible_stages[i];
            if (impl->inputAttributes.count(possible_stages[i]) != 0) {
                stage_attr.InputAttributes = impl->inputAttributes.at(possible_stages[i]);
            }
            if (impl->outputAttributes.count(possible_stages[i]) != 0) {
                stage_attr.OutputAttributes = impl->outputAttributes.at(possible_stages[i]);
            }

            if ((!stage_attr.InputAttributes.empty()) || (!stage_attr.OutputAttributes.empty())) {
                collected_attr.push_back(stage_attr);
            }
        }
        
        nlohmann::json vertex_attrs = collected_attr;
        if (!impl->pushConstants.empty()) {
            nlohmann::json push_blocks = impl->pushConstants;
            output_file << nlohmann::json{ out, vertex_attrs, push_blocks };
        }
        else {
            output_file << nlohmann::json{ out, vertex_attrs };
        }

        
        output_file.close();

    }

}