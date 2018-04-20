#pragma once
#ifndef SHADER_TOOLS_BINDING_GENERATOR_HPP
#define SHADER_TOOLS_BINDING_GENERATOR_HPP
#include "common/CommonInclude.hpp"
#include "common/Shader.hpp"
#include "DescriptorStructs.hpp"

namespace st {
    class BindingGeneratorImpl;
    class ShaderResource;

    class BindingGenerator {
        BindingGenerator(const BindingGenerator&) = delete;
        BindingGenerator& operator=(const BindingGenerator&) = delete;
    public:

        BindingGenerator();
        ~BindingGenerator();
        BindingGenerator(BindingGenerator&& other) noexcept;
        BindingGenerator& operator=(BindingGenerator&& other) noexcept;

        void ParseBinary(const Shader& shader);
        uint32_t GetNumSets() const noexcept;
        void Clear();
        void GetShaderResources(const size_t set_idx, size_t* num_resources, ShaderResource* resources);

    private:
        std::unique_ptr<BindingGeneratorImpl> impl;
    };

}

#endif //!SHADER_TOOLS_BINDING_GENERATOR_HPP