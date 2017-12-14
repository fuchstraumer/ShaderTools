#include "ShaderGroup.hpp"
#include "spirv-cross/spirv_cross.hpp"
#include "spirv-cross/spirv_glsl.hpp"
#include <fstream>
namespace st {

    uint32_t DeserializeUint32(const char (&buf)[4]) {
        uint32_t u0 = buf[0];
        uint32_t u1 = buf[1];
        uint32_t u2 = buf[2];
        uint32_t u3 = buf[3];
        uint32_t result = u0 | (u1 << 8) | (u2 << 16) | (u3 << 24);
        return result;
    }

    std::vector<uint32_t> loadFile(const std::string& path) {
        std::ifstream spirvfile(path, std::ios::binary);
        if(!spirvfile.is_open()) {
            throw std::runtime_error("Couldn't open SPIR-V file to parse for reflection data!");
        }

        std::vector<uint32_t> result;
        while(spirvfile.eof()) {
            char buf[4];
            spirvfile.get(buf, 4);
            result.push_back(DeserializeUint32(buf));
        }

        return result;
    }

    void ShaderGroup::AddShader(const std::string& raw_path, const VkShaderStageFlags& stage) {
        if(compiler.HasShader(raw_path)) {
            parseBinary(compiler.GetBinary(raw_path), stage);
        }
        else {
            compiler.Compile(raw_path, stage);
            parseBinary(compiler.GetBinary(raw_path), stage);
        }
    }

    const std::vector<uint32_t>& ShaderGroup::CompileAndAddShader(const std::string& raw_path, const VkShaderStageFlags& stage) {
        const auto& result = compiler.Compile(raw_path, stage);
        AddShader(raw_path, stage);
        return result;
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
            info.Members.insert(std::make_pair(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, std::move(obj)));
        }
    }

    void parseStorageBuffers(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.uniform_buffers) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            info.Members.insert(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, std::move(obj)));
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
            info.Members.insert(std::make_pair(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, std::move(obj)));
        }
    }

    void parseStorageImages(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.uniform_buffers) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            info.Members.insert(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::move(obj)));
        }
    }

    void parseCombinedSampledImages(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.uniform_buffers) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            info.Members.insert(std::make_pair(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::move(obj)));
        }
    }

    void parseSeparableSampledImages(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.uniform_buffers) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            info.Members.insert(std::make_pair(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, std::move(obj)));
        }
    }

    void parseSeparableSamplers(DescriptorSetInfo& info, const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {
        using namespace spirv_cross;
        const auto rsrcs = cmplr.get_shader_resources();
        for(const auto& ubuff : rsrcs.uniform_buffers) {
            DescriptorObject obj;
            obj.Stages = stage;
            obj.Binding = cmplr.get_decoration(ubuff.id, spv::DecorationBinding);
            obj.ParentSet = cmplr.get_decoration(ubuff.id, spv::DecorationDescriptorSet);
            obj.Name = cmplr.get_name(ubuff.id);
            info.Members.insert(std::make_pair(VK_DESCRIPTOR_TYPE_SAMPLER, std::move(obj)));
        }
    }

    PushConstantInfo parsePushConstants(const spirv_cross::CompilerGLSL& cmplr, const VkShaderStageFlags& stage) {

    }
    
    void ShaderGroup::parseBinary(const std::vector<uint32_t>& binary, const VkShaderStageFlags& stage) {
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
            if (!rsrcs.push_constant_buffers.empty()) {
                PushConstantInfo push_constant = parsePushConstants(glsl, stage);
                auto inserted = pushConstants.insert(std::make_pair(stage, std::move(push_constant)));
                if(!inserted.second) {
                    throw std::runtime_error("Failed to insert a push constant into storage map: is there already another object for this stage?");
                }
            }
        }
    }

    void ShaderGroup::collateBindings() {
        /*  Now, we need to check the descriptor sets in all the active stages we added and
            find all descriptor sets - and their component objects - that are used in multiple
            stages. We will then group these further by VK_DESCRIPTOR_TYPE, and use them to start
            filling out entries in our VkDescriptorSetLayoutBinding object.
        */
    }


}