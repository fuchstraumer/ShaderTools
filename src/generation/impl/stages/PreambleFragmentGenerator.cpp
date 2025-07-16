#include "PreambleFragmentGenerator.hpp"
#include <fstream>
#include <format>

namespace st
{
    PreambleFragmentGenerator::PreambleFragmentGenerator(SessionImpl& session) noexcept : FragmentGeneratorBase(FragmentType::Preamble, session)
    {}
    
    ShaderToolsErrorCode PreambleFragmentGenerator::GenerateFragment(ShaderGenerationContext& source)
    {
        std::filesystem::path preamblePath = GetBasePath() / "builtins/premable450.glsl";

        std::ifstream preambleFile(preamblePath);
        if (!preambleFile.is_open())
        {
            errorSession.AddError(this, ShaderToolsErrorSource::Generator, ShaderToolsErrorCode::GeneratorUnableToFindPreambleFile, preamblePath.string().c_str());
            return ShaderToolsErrorCode::GeneratorUnableToFindPreambleFile;
        }

        std::string preambleContent((std::istreambuf_iterator<char>(preambleFile)), std::istreambuf_iterator<char>());
        if (preambleContent.empty())
        {
            errorSession.AddError(this, ShaderToolsErrorSource::Filesystem, ShaderToolsErrorCode::FilesystemFailedToReadValidFileStream, preamblePath.string().c_str());
            return ShaderToolsErrorCode::FilesystemFailedToReadValidFileStream;
        }

        // Add a newline, preamble content, and another newline to the output
        std::string output_string = std::format("\n{}\n", preambleContent);
        source.Output.emplace(FragmentType::Preamble, output_string);

        return ShaderToolsErrorCode::Success;
    }

}
