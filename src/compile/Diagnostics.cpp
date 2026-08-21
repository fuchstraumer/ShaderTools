#include "compile/Diagnostics.hpp"

#include <cstdio>
#include <print>
#include <string>
#include <string_view>
#include <utility>

namespace lodestone
{

namespace
{

    /** `file(line,column): ` when the record names a place, and nothing when it does not. The shape
     * follows the compiler convention that an editor already knows how to click on. */
    std::string FormatLocation(const Diagnostic& diagnostic)
    {
        if (diagnostic.File.empty() || !HasLocation(diagnostic.Range))
        {
            return {};
        }

        return std::format(
            "{}({},{}): ", diagnostic.File, diagnostic.Range.StartLine, diagnostic.Range.StartColumn);
    }

    std::string FormatCode(const Diagnostic& diagnostic)
    {
        if (diagnostic.Code.empty())
        {
            return {};
        }

        return std::format(" {}", diagnostic.Code);
    }

} // namespace

std::string_view ToString(DiagnosticSeverity severity) noexcept
{
    switch (severity)
    {
    case DiagnosticSeverity::Note:
        return "note";
    case DiagnosticSeverity::Warning:
        return "warning";
    case DiagnosticSeverity::Error:
        return "error";
    case DiagnosticSeverity::Fatal:
        return "fatal error";
    case DiagnosticSeverity::Invalid:
        return "invalid";
    }

    return "invalid";
}

bool IsFailure(DiagnosticSeverity severity) noexcept
{
    return severity > DiagnosticSeverity::Warning;
}

bool HasLocation(const SourceRange& range) noexcept
{
    return range.StartLine != 0 || range.StartColumn != 0;
}

void StderrDiagnosticSink::Report(const Diagnostic& diagnostic)
{
    if (IsFailure(diagnostic.Severity))
    {
        ++failureCount;
    }

    std::println(stderr,
                 "[shader_cooker] {}{}{}: {} [{}]",
                 FormatLocation(diagnostic),
                 ToString(diagnostic.Severity),
                 FormatCode(diagnostic),
                 diagnostic.Message,
                 diagnostic.Context);

    for (const DiagnosticNote& note : diagnostic.Related)
    {
        if (note.File.empty() || !HasLocation(note.Range))
        {
            std::println(stderr, "[shader_cooker]   note: {}", note.Message);
            continue;
        }

        std::println(stderr,
                     "[shader_cooker]   {}({},{}): note: {}",
                     note.File,
                     note.Range.StartLine,
                     note.Range.StartColumn,
                     note.Message);
    }
}

int32_t StderrDiagnosticSink::FailureCount() const noexcept
{
    return failureCount;
}

void RecordingDiagnosticSink::Report(const Diagnostic& diagnostic)
{
    records.push_back(diagnostic);
}

const std::vector<Diagnostic>& RecordingDiagnosticSink::Records() const noexcept
{
    return records;
}

} // namespace lodestone
