#pragma once
#ifndef SHADERTOOLS_PREAMBLE_FRAGMENT_GENERATOR_HPP
#define SHADERTOOLS_PREAMBLE_FRAGMENT_GENERATOR_HPP
#include "FragmentGeneratorBase.hpp"

namespace st
{

    class PreambleFragmentGenerator : public FragmentGeneratorBase
    {
    public:

        PreambleFragmentGenerator(SessionImpl& session) noexcept;
        ShaderToolsErrorCode GenerateFragment(ShaderGenerationContext& source) override;

    };

}

#endif // !SHADERTOOLS_PREAMBLE_FRAGMENT_GENERATOR_HPP
