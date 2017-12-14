#pragma once 
#ifndef SHADER_TOOLS_GROUP_HPP
#define SHADER_TOOLS_GROUP_HPP
#include "Compiler.hpp"
#include "ShaderStructs.hpp"

namespace st {

    class ShaderGroup {
    public:
        ShaderGroup() = default;
        ~ShaderGroup() = default;

        void AddShader(const std::string& compiled_shader_path, const VkShaderStageFlags& stages);
        const std::vector<uint32_t>& CompileAndAddShader(const std::string& uncompiled_shader_path, const VkShaderStageFlags& stages);
        
        const std::vector<VkDescriptorSetLayoutBinding>& GetLayoutBindings();
        const std::vector<VkPushConstantRange>& GetPushConstantRanges();
    private:

        void parseBinary(const std::vector<uint32_t>& binary, const VkShaderStageFlags& stage);
        void collateBindings();
        ShaderCompiler compiler;
        // Reason for difference: we can have multiple sets per shader stage, but we're currently
        // only allowed to have one VkPushConstantRange per shader stage.
        std::unordered_multimap<VkShaderStageFlags, DescriptorSetInfo> descriptorSets;
        std::unordered_map<VkShaderStageFlags, PushConstantInfo> pushConstants;
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::vector<VkPushConstantRange> ranges;
    };

}

#endif //!SHADER_TOOLS_GROUP_HPP