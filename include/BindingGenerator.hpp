#pragma once
#ifndef SHADER_TOOLS_BINDING_GENERATOR_HPP
#define SHADER_TOOLS_BINDING_GENERATOR_HPP
#include "CommonInclude.hpp"

namespace st {
    class BindingGeneratorImpl;

    class ST_API BindingGenerator {
        BindingGenerator(const BindingGenerator&) = delete;
        BindingGenerator& operator=(const BindingGenerator&) = delete;
    public:

        BindingGenerator();
        ~BindingGenerator();
        BindingGenerator(BindingGenerator&& other) noexcept;
        BindingGenerator& operator=(BindingGenerator&& other) noexcept;

        void ParseBinary(const uint32_t binary_size, const uint32_t* binary, const VkShaderStageFlags stage);
        void CollateBindings();

        size_t GetNumSets() const noexcept;
        void GetLayoutBindings(const size_t& set_index, uint32_t* num_bindings, VkDescriptorSetLayoutBinding* bindings) const;
        void GetPushConstantRanges(uint32_t* num_ranges, VkPushConstantRange* ranges) const;
        void GetVertexAttributes(uint32_t* num_attrs, VkVertexInputAttributeDescription* attrs) const;
        void GetVertexBindings(uint32_t* num_bindings, VkVertexInputBindingDescription* bindings) const;

        void SaveToJSON(const char* output_name);
        void LoadFromJSON(const char* input_name);

    private:
        std::unique_ptr<BindingGeneratorImpl> impl;
    };

}

#endif //!SHADER_TOOLS_BINDING_GENERATOR_HPP