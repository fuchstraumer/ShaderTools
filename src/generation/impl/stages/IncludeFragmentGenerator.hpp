#pragma once
#ifndef SHADERTOOLS_INCLUDE_FRAGMENT_GENERATOR_HPP
#define SHADERTOOLS_INCLUDE_FRAGMENT_GENERATOR_HPP
#include "FragmentGeneratorBase.hpp"
#include <regex>

namespace st
{
    static const std::regex include_library("#include <(\\S+)>\n");
    static const std::regex include_local("#include \"(\\S+)\"\n");

    /**
     * @brief Fragment generator that verifies include paths, adding them to the output fragments and removing them from the input source
     */
    class IncludeFragmentGenerator : public FragmentGeneratorBase
    {
    public:

        IncludeFragmentGenerator(SessionImpl& session) noexcept;
        ShaderToolsErrorCode GenerateFragment(ShaderGenerationContext& source) override;

    };

}

#endif // !SHADERTOOLS_INCLUDE_FRAGMENT_GENERATOR_HPP
