#include "SpecializationConstantFragmentGenerator.hpp"
#include <regex>
#include <format>

namespace st
{

    static const std::regex specialization_constant_regex("#pragma\\s+SPC\\s+(const.*$)");

    SpecializationConstantFragmentGenerator::SpecializationConstantFragmentGenerator(SessionImpl& session) noexcept
        : FragmentGeneratorBase(FragmentType::SpecConstant, session) {}

    ShaderToolsErrorCode SpecializationConstantFragmentGenerator::GenerateFragment(ShaderGenerationContext& source)
    {
        bool spc_found = true;
        while (spc_found)
        {
            std::smatch spc_match;
            if (std::regex_search(source.Input, spc_match, specialization_constant_regex))
            {
                // post-increment is important here, we want the current index before incrementing
                uint32_t constant_id = source.ResourcesInfo.LastConstantIndex++;
                std::string spc_prefix = std::format("layout (constant_id = {}) ", constant_id);
                std::string spc_string = spc_prefix + spc_match[1].str() + "\n";
                auto iter = source.Output.emplace(FragmentType::SpecConstant, spc_string);
                if (iter == source.Output.end())
                {
                    std::string error_message = std::format("Failed to store SPC {} with ID {}", spc_match[1].str(), constant_id);
                    errorSession.AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorUnableToStoreFragmentFileContents, error_message.c_str());
                    return ShaderToolsErrorCode::GeneratorUnableToStoreFragmentFileContents;
                }
                
                source.Input.erase(source.Input.begin() + spc_match.position(), source.Input.begin() + spc_match.position() + spc_match.length());
            }
            else
            {
                spc_found = false;
            }
        }

        return ShaderToolsErrorCode::Success;
    }

}
