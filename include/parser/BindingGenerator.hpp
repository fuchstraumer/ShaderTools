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

        uint32_t GetNumSets() const noexcept;

        void Clear();

    private:
        std::unique_ptr<BindingGeneratorImpl> impl;
    };

}

#endif //!SHADER_TOOLS_BINDING_GENERATOR_HPP