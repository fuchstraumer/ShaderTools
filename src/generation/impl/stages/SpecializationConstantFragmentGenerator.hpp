#pragma once
#ifndef SHADERTOOLS_SPECIALIZATION_CONSTANT_FRAGMENT_GENERATOR_HPP
#define SHADERTOOLS_SPECIALIZATION_CONSTANT_FRAGMENT_GENERATOR_HPP
#include "FragmentGeneratorBase.hpp"

namespace st
{

    class SpecializationConstantFragmentGenerator : public FragmentGeneratorBase
    {
    public:

        SpecializationConstantFragmentGenerator(SessionImpl& session) noexcept;
        ShaderToolsErrorCode GenerateFragment(ShaderGenerationContext& source) override;

    };

}

#endif // !SHADERTOOLS_SPECIALIZATION_CONSTANT_FRAGMENT_GENERATOR_HPP
