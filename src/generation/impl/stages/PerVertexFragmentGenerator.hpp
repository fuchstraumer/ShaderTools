#pragma once
#ifndef SHADERTOOLS_PERVERTEX_GENERATOR_HPP
#define SHADERTOOLS_PERVERTEX_GENERATOR_HPP
#include "FragmentGeneratorBase.hpp"

namespace st
{

    /**
     * @brief Fragment generator that adds the stage interfaces to a given fragment. For vertex shaders, it will also define
     * the built-in gl_PerVertex block, otherwise it pulls from parsed data to find the interface information it needs to generate
     * the interface block.
     */
    class InterfaceBlockGenerator : public FragmentGeneratorBase
    {
    public:
        InterfaceBlockGenerator(SessionImpl& session) noexcept : FragmentGeneratorBase(FragmentType::InterfaceBlock, session) {}

        ShaderToolsErrorCode GenerateFragment(ShaderGenerationContext& source) override;
    };

}

#endif // !SHADERTOOLS_PERVERTEX_GENERATOR_HPP