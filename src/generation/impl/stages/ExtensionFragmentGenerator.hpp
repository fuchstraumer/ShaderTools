#pragma once
#ifndef SHADERTOOLS_EXTENSION_FRAGMENT_GENERATOR_HPP
#define SHADERTOOLS_EXTENSION_FRAGMENT_GENERATOR_HPP
#include "FragmentGeneratorBase.hpp"
#include <string>
#include <vector>
#include <format>

namespace st
{

    class ExtensionFragmentGenerator : public FragmentGeneratorBase
    {
    public:

        ExtensionFragmentGenerator(const std::vector<std::string>& extensions, SessionImpl& session) noexcept;
        ShaderToolsErrorCode GenerateFragment(ShaderGenerationContext& source) override;

    private:

        std::vector<std::string> extensionStrings;
        // Example of how to format the output string for extensions
        ShaderToolsErrorCode addExtensionsToOutput(ShaderGenerationContext& source);

    };

}

#endif // !SHADERTOOLS_EXTENSION_FRAGMENT_GENERATOR_HPP
