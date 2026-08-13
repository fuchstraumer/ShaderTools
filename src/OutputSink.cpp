#include "OutputSink.hpp"
#include <fstream>

namespace velox::cooker
{

OutputSink::OutputSink() noexcept = default;
OutputSink::~OutputSink() = default;

FileOutputSink::FileOutputSink(std::filesystem::path path) :
    path{ std::move(path) },
    description{},
    primaryName{}
{
    description = this->path.string();
    primaryName = this->path.filename().string();
}

FileOutputSink::~FileOutputSink() = default;

CookResult<void> FileOutputSink::WriteArtifact(std::string_view artifact_name, std::string_view content)
{
    const std::filesystem::path artifactPath = path.parent_path() / std::filesystem::path{ artifact_name };
    FileOutputSink companion{ artifactPath };
    return companion.Write(content);
}

std::string_view FileOutputSink::PrimaryName() const noexcept
{
    return primaryName;
}

CookResult<void> FileOutputSink::Write(std::string_view content)
{
    const std::filesystem::path parentDirectory = path.parent_path();
    if (!parentDirectory.empty() && !std::filesystem::exists(parentDirectory))
    {
        std::filesystem::create_directories(parentDirectory);
    }

    if (!parentDirectory.empty() && !std::filesystem::is_directory(parentDirectory))
    {
        return std::unexpected(CookError::OutputPathInvalid);
    }

    std::ofstream stream{ path, std::ios::binary | std::ios::trunc };
    if (!stream.is_open())
    {
        return std::unexpected(CookError::OutputWriteFailed);
    }

    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!stream.good())
    {
        return std::unexpected(CookError::OutputWriteFailed);
    }

    return {};
}

std::string_view FileOutputSink::Describe() const noexcept
{
    return description;
}

MemoryOutputSink::MemoryOutputSink() noexcept = default;
MemoryOutputSink::~MemoryOutputSink() = default;

CookResult<void> MemoryOutputSink::Write(std::string_view new_content)
{
    content.assign(new_content);
    return {};
}

CookResult<void> MemoryOutputSink::WriteArtifact(std::string_view artifact_name, std::string_view content)
{
    artifacts[std::string{ artifact_name }] = std::string{ content };
    return {};
}

std::string_view MemoryOutputSink::Describe() const noexcept
{
    return "<memory>";
}

std::string_view MemoryOutputSink::PrimaryName() const noexcept
{
    return "ShaderLibrary.hpp";
}

const std::map<std::string, std::string>& MemoryOutputSink::GetArtifacts() const noexcept
{
    return artifacts;
}

std::string_view MemoryOutputSink::GetContent() const noexcept
{
    return content;
}

} // namespace velox::cooker
