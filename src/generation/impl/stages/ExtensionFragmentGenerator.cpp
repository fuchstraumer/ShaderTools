#include "ExtensionFragmentGenerator.hpp"

namespace st
{

    ExtensionFragmentGenerator::ExtensionFragmentGenerator(const std::vector<std::string>& extensions, SessionImpl& session) noexcept
        : FragmentGeneratorBase(FragmentType::Extension, session), extensionStrings(extensions)
    {}

    ShaderToolsErrorCode ExtensionFragmentGenerator::GenerateFragment(ShaderGenerationContext& source)
    {
        if (extensionStrings.empty())
        {
            return ShaderToolsErrorCode::ExtensionFragmentGeneratorNoExtensionsProvided;
        }

        return addExtensionsToOutput(source);
    }

    ShaderToolsErrorCode ExtensionFragmentGenerator::addExtensionsToOutput(ShaderGenerationContext& source)
    {
        // prepend output with newline
        std::string output_string = "\n";

        for (const auto& ext : extensionStrings)
        {
            output_string += std::format("#extension {} : require\n", ext);
        }

        // append newline to the end
        output_string += "\n";

        auto iter = source.Output.emplace(FragmentType::Extension, output_string);
        if (iter == source.Output.end())
        {
            errorSession.AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorUnableToStoreFragmentFileContents, "Failed to store extension fragment");
            return ShaderToolsErrorCode::GeneratorUnableToStoreFragmentFileContents;
        }

        return ShaderToolsErrorCode::Success;
    }

}