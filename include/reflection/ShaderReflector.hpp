#pragma once
#ifndef SHADER_TOOLS_BINDING_GENERATOR_HPP
#define SHADER_TOOLS_BINDING_GENERATOR_HPP
#include "common/CommonInclude.hpp"
#include "common/Shader.hpp"

namespace st {

    class ShaderGroup;
    class ShaderGroupImpl;
    class ShaderReflectorImpl;
    class ResourceUsage;

    class ShaderReflector {
        ShaderReflector(const ShaderReflector&) = delete;
        ShaderReflector& operator=(const ShaderReflector&) = delete;
    public:

        ShaderReflector();
        ~ShaderReflector();
        ShaderReflector(ShaderReflector&& other) noexcept;
        ShaderReflector& operator=(ShaderReflector&& other) noexcept;

        void ParseBinary(const Shader& shader);
        uint32_t GetNumSets() const noexcept;
        void Clear();
        void GetShaderResources(const size_t set_idx, size_t* num_resources, ResourceUsage* resources);

        friend class ShaderGroup;
        friend class ShaderGroupImpl;
    protected:
        ShaderReflectorImpl* GetImpl();
    private:
        std::unique_ptr<ShaderReflectorImpl> impl;
    };

}

#endif //!SHADER_TOOLS_BINDING_GENERATOR_HPP
