#include "BindingGenerator.hpp"
#include "spirv-cross/spirv_cross.hpp"
#include "spirv-cross/spirv_glsl.hpp"
#include <fstream>
#include <filesystem>

namespace st {

    size_t BindingGenerator::GetNumSets() const noexcept {
        return sortedSets.size();
    }

    std::vector<VkDescriptorSetLayoutBinding> BindingGenerator::GetLayoutBindings(const size_t& idx) const {
        auto& set = sortedSets[idx];
        std::vector<VkDescriptorSetLayoutBinding> result;
        for(auto& member : set.Members) {
            result.push_back((VkDescriptorSetLayoutBinding)member);
        }
        return result;
    }

    void BindingGenerator::SaveToJSON(const std::string & output_name) {

        if (sortedSets.empty()) {
            CollateBindings();
        }

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
        out = sortedSets;
        output_file << out;
        output_file.close();
        
    }

    void BindingGenerator::LoadFromJSON(const std::string& input) {

        std::ifstream input_file(input);
        if(!input_file.is_open()) {
            throw std::domain_error("BindingGenerator failed to open input file for reading!");
        }

        nlohmann::json j;
        input_file >> j;
        sortedSets = j;
        input_file.close();
    }

    
    void parseUniformBuffers(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
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
                member.Size = range.range;
                member.Offset = range.offset;
                obj.Members.push_back(std::move(member));
            }
            obj.Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            info.Members.push_back(std::move(obj));
        }
    }

    void parseStorageBuffers(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
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
                ShaderDataObject member;
                member.Name = cmplr.get_member_name(sbuff.base_type_id, member.index);
                member.Size = member.range;
                member.Offset = member.offset;
                obj.Members.push_back(member);
            }
            obj.Type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            info.Members.push_back(std::move(obj));
        }
    }

    void parseInputAttachments(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.subpass_inputs) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            obj.Type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            info.Members.push_back(std::move(obj));
        }
    }

    void parseStorageImages(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.storage_images) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            obj.Type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            info.Members.push_back(std::move(obj));
        }
    }

    void parseCombinedSampledImages(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.sampled_images) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            obj.Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            info.Members.push_back(std::move(obj));
        }
    }

    void parseSeparableSampledImages(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.separate_images) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            obj.Type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            info.Members.push_back(std::move(obj));
        }
    }

    void parseSeparableSamplers(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.separate_samplers) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            obj.Type = VK_DESCRIPTOR_TYPE_SAMPLER;
            info.Members.push_back(std::move(obj));
        }
    }

    PushConstantInfo parsePushConstants(const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
        const auto push_constants = cmplr.get_shader_resources();
        const auto& pconstant = push_constants.push_constant_buffers.front();
        auto ranges = cmplr.get_active_buffer_ranges(pconstant.id);
        PushConstantInfo result;
        result.Stages = stage;
        result.Name = cmplr.get_name(pconstant.id);
        for(auto& range : ranges) {
            ShaderDataObject member;
            member.Name = cmplr.get_member_name(pconstant.base_type_id, range.index);
            member.Size = range.range;
            member.Offset = range.offset;
            result.Members.push_back(std::move(member));
        }
        return result;
    }
    
    void BindingGenerator::ParseBinary(const std::vector<uint32_t>& binary, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        CompilerGLSL glsl(binary);
        DescriptorSetInfo info;
        parseUniformBuffers(info, glsl, stage);
        parseStorageBuffers(info, glsl, stage);
        parseInputAttachments(info, glsl, stage);
        parseStorageImages(info, glsl, stage);
        parseCombinedSampledImages(info, glsl, stage);
        parseSeparableSampledImages(info, glsl, stage);
        parseSeparableSamplers(info, glsl, stage);
        descriptorSets.insert(std::make_pair(stage, std::move(info)));
        { 
            const auto rsrcs = glsl.get_shader_resources();
            PushConstantInfo push_constant = parsePushConstants(glsl, stage);
            auto inserted = pushConstants.insert(std::make_pair(stage, std::move(push_constant)));
        }

    }

    void BindingGenerator::CollateBindings() {
        /*  Now, we need to check the descriptor sets in all the active stages we added and
            find all descriptor sets - and their component objects - that are used in multiple
            stages. We will then group these further by VK_DESCRIPTOR_TYPE, and use them to start
            filling out entries in our VkDescriptorSetLayoutBinding object.
        */

        if(!sortedSets.empty()) {
            // Clear these, as we have to resort 
            // from the raw imported/parsed data 
            sortedSets.clear();
        }

        for (const auto& entry : descriptorSets) {
            for (const auto& obj : entry.second.Members) {
                const auto& set_idx = obj.ParentSet;
                if (set_idx + 1 > sortedSets.size()) {
                    sortedSets.resize(set_idx + 1);
                }
                
                // Check to see if set at index has been created yet.
                if (sortedSets[set_idx].Index == std::numeric_limits<uint32_t>::max()) {
                    sortedSets[set_idx].Index = set_idx;
                }

                const auto& binding_idx = obj.Binding;
                auto& set = sortedSets[set_idx];
                if (binding_idx + 1 > set.Members.size()) {
                    set.Members.resize(binding_idx + 1);
                    set.Members[binding_idx] = obj;
                }
                else if (set.Members[binding_idx].Type == VK_DESCRIPTOR_TYPE_MAX_ENUM) {
                    set.Members[binding_idx] = obj;
                }
                else if (set.Members[binding_idx].Name.empty()) {
                    set.Members[binding_idx] = obj;
                }
                else {
                    // Already exists.
                    if (set.Members[binding_idx].Binding == obj.Binding) {
                        if (set.Members[binding_idx].Type != obj.Type) {
                            throw std::domain_error("Two descriptors objects in the same set and at the same binding location have differing types!");
                        }
                        else {
                            // OR stage flags together and move on.
                            set.Members[binding_idx].Stages |= obj.Stages;
                        }
                    }
                }
                
                
                    
                
            }
        }

    }

}