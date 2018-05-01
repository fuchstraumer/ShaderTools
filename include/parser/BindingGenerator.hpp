#pragma once
#ifndef SHADER_TOOLS_BINDING_GENERATOR_HPP
#define SHADER_TOOLS_BINDING_GENERATOR_HPP
#include "common/CommonInclude.hpp"
#include "common/Shader.hpp"

namespace st {

    class ShaderGroup;
    class ShaderGroupImpl;
    class BindingGeneratorImpl;
    class ResourceUsage;

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
        void GetShaderResources(const size_t set_idx, size_t* num_resources, ResourceUsage* resources);

        friend class ShaderGroup;
        friend class ShaderGroupImpl;
    protected:
        BindingGeneratorImpl * GetImpl();
    private:
        std::unique_ptr<BindingGeneratorImpl> impl;
    };

}

#endif //!SHADER_TOOLS_BINDING_GENERATOR_HPP