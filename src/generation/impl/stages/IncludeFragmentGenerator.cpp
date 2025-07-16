#include "IncludeFragmentGenerator.hpp"

namespace st
{

    IncludeFragmentGenerator::IncludeFragmentGenerator(SessionImpl& session) noexcept : FragmentGeneratorBase(FragmentType::IncludePath, session)
    {}

    ShaderToolsErrorCode IncludeFragmentGenerator::GenerateFragment(ShaderGenerationContext& source)
    {
        bool include_found = true;

        std::vector<std::string> include_path_strs;

        while (include_found)
        {
            std::smatch match;
            if (std::regex_search(source.Input, match, include_local))
            {
                include_path_strs.emplace_back(match[0].str());
                source.Input.erase(source.Input.begin() + match.position(), source.Input.begin() + match.position() + match.length());
            }
            else if (std::regex_search(source.Input, match, include_library))
            {
                include_path_strs.emplace_back(match[0].str());
                source.Input.erase(source.Input.begin() + match.position(), source.Input.begin() + match.position() + match.length());
            }
            else
            {
                include_found = false;
            }
        }

        for (auto&& include_path : include_path_strs)
        {
            auto iter = source.Output.emplace(FragmentType::IncludePath, std::move(include_path));
            if (iter == source.Output.end())
            {
                errorSession.AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorUnableToStoreFragmentFileContents, "Failed to store include fragment");
                return ShaderToolsErrorCode::GeneratorUnableToStoreFragmentFileContents;
            }
        }

        return ShaderToolsErrorCode::Success;
    }

}