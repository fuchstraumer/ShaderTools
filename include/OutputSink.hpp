#pragma once
#ifndef VELOX_SHADER_COOKER_OUTPUT_SINK_HPP
#define VELOX_SHADER_COOKER_OUTPUT_SINK_HPP
#include "CookerErrors.hpp"
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

/** Where cooked output goes. Kept behind an interface so a future watch-and-serve process can hand
 * sources to a running engine without going through the filesystem. */
namespace lodestone
{

class OutputSink
{
public:
    OutputSink() noexcept;
    virtual ~OutputSink();
    OutputSink(const OutputSink&) = delete;
    OutputSink& operator=(const OutputSink&) = delete;
    OutputSink(OutputSink&&) noexcept = default;
    OutputSink& operator=(OutputSink&&) noexcept = default;

    /** Writes the primary artifact, which is the generated header. */
    virtual CookResult<void> Write(std::string_view content) = 0;
    /** Writes a companion artifact beside the primary one. The name is a file name, not a path. */
    virtual CookResult<void> WriteArtifact(std::string_view artifact_name,
                                           std::string_view content) = 0;
    [[nodiscard]] virtual std::string_view Describe() const noexcept = 0;
    /** File name of the primary artifact, so a companion can include it. */
    [[nodiscard]] virtual std::string_view PrimaryName() const noexcept = 0;
};

class FileOutputSink final : public OutputSink
{
public:
    explicit FileOutputSink(std::filesystem::path path);
    ~FileOutputSink() override;
    FileOutputSink(FileOutputSink&&) noexcept = default;
    FileOutputSink& operator=(FileOutputSink&&) noexcept = default;

    [[nodiscard]] CookResult<void> Write(std::string_view content) override;
    [[nodiscard]] CookResult<void> WriteArtifact(std::string_view artifact_name, std::string_view content) override;
    [[nodiscard]] std::string_view Describe() const noexcept override;
    [[nodiscard]] std::string_view PrimaryName() const noexcept override;

private:
    std::filesystem::path path;
    std::string description;
    std::string primaryName;
};

class MemoryOutputSink final : public OutputSink
{
public:
    MemoryOutputSink() noexcept;
    ~MemoryOutputSink() override;

    [[nodiscard]] CookResult<void> Write(std::string_view content) override;
    [[nodiscard]] CookResult<void> WriteArtifact(std::string_view artifact_name, std::string_view content) override;
    [[nodiscard]] std::string_view Describe() const noexcept override;
    [[nodiscard]] std::string_view PrimaryName() const noexcept override;
    [[nodiscard]] std::string_view GetContent() const noexcept;
    /** Every companion artifact, keyed by name. The determinism check compares two cooks with it. */
    [[nodiscard]] const std::map<std::string, std::string>& GetArtifacts() const noexcept;

private:
    std::string content;
    std::map<std::string, std::string> artifacts;
};

} // namespace lodestone

#endif // !VELOX_SHADER_COOKER_OUTPUT_SINK_HPP
