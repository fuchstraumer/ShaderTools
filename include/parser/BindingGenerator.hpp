#pragma once
#ifndef SHADER_TOOLS_BINDING_GENERATOR_HPP
#define SHADER_TOOLS_BINDING_GENERATOR_HPP
#include "common/CommonInclude.hpp"
#include "common/Shader.hpp"
#include "DescriptorStructs.hpp"
#include "ShaderResource.hpp"

namespace st {
    class BindingGeneratorImpl;

    class BindingGenerator {
        BindingGenerator(const BindingGenerator&) = delete;
        BindingGenerator& operator=(const BindingGenerator&) = delete;
    public:

        BindingGenerator();
        ~BindingGenerator();
        BindingGenerator(BindingGenerator&& other) noexcept;
        BindingGenerator& operator=(BindingGenerator&& other) noexcept;

        void ParseBinary(const Shader& shader);
        void ParseBinary(const std::vector<uint32_t>& shader_binary, const VkShaderStageFlags stage);

        uint32_t GetNumSets() const noexcept;
        std::map<uint32_t, ShaderResource> GetDescriptorSetObjects(const uint32_t& set_idx) const;
        std::map<std::string, VkDescriptorSetLayoutBinding> GetSetNameBindingPairs(const uint32_t& set_idx) const;
        std::vector<VkPushConstantRange> GetPushConstantRanges() const;
        std::vector<VkVertexInputAttributeDescription> GetVertexAttributes() const;
        void Clear();

    private:
        std::unique_ptr<BindingGeneratorImpl> impl;
    };

}

#endif //!SHADER_TOOLS_BINDING_GENERATOR_HPP