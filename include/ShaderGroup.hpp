#pragma once 
#ifndef SHADER_TOOLS_GROUP_HPP
#define SHADER_TOOLS_GROUP_HPP
#include "Compiler.hpp"
#include "ShaderStructs.hpp"
#include <map>
namespace st {

    class ShaderGroup {
    public:
        ShaderGroup() = default;
        ~ShaderGroup() = default;

        void AddShader(const std::string& compiled_shader_path, const VkShaderStageFlags& stages);
        const std::vector<uint32_t>& CompileAndAddShader(const std::string& uncompiled_shader_path, const VkShaderStageFlags& stages);
        
        const size_t& GetNumSets() const noexcept;
        std::vector<VkDescriptorSetLayoutBinding> GetLayoutBindings(const size_t& set_index) const noexcept;
        const std::vector<VkPushConstantRange>& GetPushConstantRanges();
    private:

        void parseBinary(const std::vector<uint32_t>& binary, const VkShaderStageFlags& stage);
        void collateBindings();
        ShaderCompiler compiler;
        // Reason for difference: we can have multiple sets per shader stage, but we're currently
        // only allowed to have one VkPushConstantRange per shader stage.
        std::unordered_multimap<VkShaderStageFlags, DescriptorSetInfo> descriptorSets;
        std::unordered_map<VkShaderStageFlags, PushConstantInfo> pushConstants;
        using descriptor_pack_t = std::map<size_t, VkDescriptorSetLayoutBinding>;
        // Corresponds to all sets and each of their unique packs of bound objects across all shader stages.
        std::vector<descriptor_pack_t> bindings;
        std::vector<VkPushConstantRange> ranges;
    };

}

#endif //!SHADER_TOOLS_GROUP_HPP