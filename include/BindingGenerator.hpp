#pragma once
#ifndef SHADER_TOOLS_BINDING_GENERATOR_HPP
#define SHADER_TOOLS_BINDING_GENERATOR_HPP
#include "ShaderStructs.hpp"
#include <unordered_map>

namespace st {

    class BindingGenerator {
    public:

        void ParseBinary(const std::vector<uint32_t>& binary, const VkShaderStageFlags& stage);
        void CollateBindings();

        size_t GetNumSets() const noexcept;
        std::vector<VkDescriptorSetLayoutBinding> GetLayoutBindings(const size_t& set_index) const;
        std::vector<VkPushConstantRange> GetPushConstantRanges();

        void SaveToJSON(const std::string& output_name);
        void LoadFromJSON(const std::string& input_name);

    private:
        std::unordered_multimap<VkShaderStageFlags, DescriptorSetInfo> descriptorSets;
        std::vector<DescriptorSetInfo> sortedSets;
        std::unordered_map<VkShaderStageFlags, PushConstantInfo> pushConstants; 
    };

}

#endif //!SHADER_TOOLS_BINDING_GENERATOR_HPP