#include "CookerOptions.hpp"
#include "CookerErrors.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string_view>
#include <system_error>

namespace lodestone
{

namespace
{

    constexpr std::string_view k_UsageText =
        "Usage: lodestone --output <header.hpp> [--O<level>] [--no-validate] [--quiet]\n"
        "                 [--cache-dir <path>] [--single-threaded] [--no-dedupe]\n"
        "                 [--verify-deterministic] [--dump-stage=<name>] <module.slang>...\n"
        "  --output, -o    destination header path (required)\n"
        "  --O<level>      slang optimization level: 0-3, defaults to 0\n"
        "  --no-validate   skip cross-checking reflection against emitted WGSL\n"
        "  --quiet         suppress the per-variant reflection report\n"
        "  --cache-dir     directory for precompiled slang modules\n"
        "  --single-threaded disable multi-threaded entry point codegen\n"
        "  --no-dedupe     disable content deduplication\n"
        "  --verify-deterministic cook twice and compare all artifacts\n"
        "  --dump-stage=<name> write one stage of the pipeline as JSON, beside the other artifacts.\n"
        "                  Repeat the flag for more than one stage. Names: space, variants, raw,\n"
        "                  resolved, interned, cooked, all. raw, resolved, and interned parse but\n"
        "                  have no boundary type yet, so they write nothing.\n";

    constexpr std::string_view k_OptimizationPrefix = "--O";
    constexpr std::string_view k_StageDumpPrefix = "--dump-stage=";
    constexpr std::string_view k_AllStageDumpsName = "all";

    /** The one table that decides both what `--dump-stage` accepts and what a dump artifact is
     * called. A separate spelling for either job would let the flag and the file name drift. */
    struct StageDumpName
    {
        StageDumpKind Kind;
        std::string_view Name;
    };

    constexpr std::array<StageDumpName, 6u> k_StageDumpNames{
        StageDumpName{ .Kind = StageDumpKind::Space, .Name = "space" },
        StageDumpName{ .Kind = StageDumpKind::Variants, .Name = "variants" },
        StageDumpName{ .Kind = StageDumpKind::Raw, .Name = "raw" },
        StageDumpName{ .Kind = StageDumpKind::Resolved, .Name = "resolved" },
        StageDumpName{ .Kind = StageDumpKind::Interned, .Name = "interned" },
        StageDumpName{ .Kind = StageDumpKind::Cooked, .Name = "cooked" }
    };

    CookResult<uint32_t> ParseStageDumpArgument(std::string_view argument)
    {
        const std::string_view name = argument.substr(k_StageDumpPrefix.size());
        if (name == k_AllStageDumpsName)
        {
            return AllStageDumpBits();
        }

        const StageDumpKind kind = ParseStageDumpKind(name);
        if (kind == StageDumpKind::Invalid)
        {
            return std::unexpected(CookError::MalformedArgument);
        }

        return StageDumpBit(kind);
    }

// ignore -Wunsafe-buffer-usage because from_chars with string_view is safe, and the warning is a little
// paranoid (as it should be)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif
    CookResult<uint32_t> ParseOptimizationLevel(std::string_view argument)
    {
        const std::string_view levelText = argument.substr(k_OptimizationPrefix.size());
        if (levelText.empty())
        {
            return std::unexpected(CookError::MalformedArgument);
        }

        uint32_t level = 0u;
        const std::from_chars_result result =
            std::from_chars(levelText.data(), levelText.data() + levelText.size(), level);
        if (result.ec != std::errc{} || level > 3u)
        {
            return std::unexpected(CookError::MalformedArgument);
        }

        return level;
    }
#ifdef __clang__
#pragma clang diagnostic pop
#endif

    std::filesystem::path DefaultModuleCacheDirectory()
    {
        return std::filesystem::temp_directory_path() / "LodestoneShaderCooker";
    }

    /** A flag that takes no argument. The table keeps the parser flat: one row for each switch, and
     * the loop below stays a lookup rather than a chain of comparisons. */
    struct SwitchFlag
    {
        std::string_view Name;
        void (*Apply)(CookerOptions& options) noexcept;
    };

    constexpr std::array<SwitchFlag, 5u> k_SwitchFlags{ SwitchFlag{ .Name = "--no-dedupe",
                                                                    .Apply =
                                                                        [](CookerOptions& options) noexcept
                                                                    {
                                                                        options.DedupeEnabled = false;
                                                                    } },
                                                        SwitchFlag{ .Name = "--verify-deterministic",
                                                                    .Apply =
                                                                        [](CookerOptions& options) noexcept
                                                                    {
                                                                        options.VerifyDeterministic = true;
                                                                    } },
                                                        SwitchFlag{
                                                            .Name = "--no-validate",
                                                            .Apply =
                                                                [](CookerOptions& options) noexcept
                                                            {
                                                                options.ValidateReflectionAgainstWgsl = false;
                                                            } },
                                                        SwitchFlag{ .Name = "--quiet",
                                                                    .Apply =
                                                                        [](CookerOptions& options) noexcept
                                                                    {
                                                                        options.ReportReflection = false;
                                                                    } },
                                                        SwitchFlag{
                                                            .Name = "--single-threaded",
                                                            .Apply = [](CookerOptions& options) noexcept
                                                            {
                                                                options.MultithreadEntryPointCodegen = false;
                                                            } } };

    const SwitchFlag* FindSwitchFlag(std::string_view argument) noexcept
    {
        for (const SwitchFlag& flag : k_SwitchFlags)
        {
            if (flag.Name == argument)
            {
                return &flag;
            }
        }

        return nullptr;
    }

    /** A flag that consumes the next argument. It cannot join the switch table, because it moves the
     * loop index. */
    CookResult<std::filesystem::path> ReadPathArgument(std::span<const std::string_view> arguments,
                                                       size_t& index)
    {
        if (index + 1u >= arguments.size())
        {
            return std::unexpected(CookError::MalformedArgument);
        }

        ++index;
        return std::filesystem::path{ arguments[index] };
    }

} // namespace

std::string_view ToString(StageDumpKind kind) noexcept
{
    for (const StageDumpName& entry : k_StageDumpNames)
    {
        if (entry.Kind == kind)
        {
            return entry.Name;
        }
    }

    return "invalid";
}

StageDumpKind ParseStageDumpKind(std::string_view name) noexcept
{
    for (const StageDumpName& entry : k_StageDumpNames)
    {
        if (entry.Name == name)
        {
            return entry.Kind;
        }
    }

    return StageDumpKind::Invalid;
}

uint32_t StageDumpBit(StageDumpKind kind) noexcept
{
    if (kind == StageDumpKind::Invalid)
    {
        return 0u;
    }

    return 1u << (static_cast<uint32_t>(kind) - 1u);
}

uint32_t AllStageDumpBits() noexcept
{
    uint32_t mask = 0u;
    for (const StageDumpName& entry : k_StageDumpNames)
    {
        mask |= StageDumpBit(entry.Kind);
    }

    return mask;
}

bool IsStageDumpRequested(const CookerOptions& options, StageDumpKind kind) noexcept
{
    return (options.DumpStageMask & StageDumpBit(kind)) != 0u;
}

CookResult<CookerOptions> ParseCommandLine(std::span<const std::string_view> arguments)
{
    CookerOptions options;
    options.ModuleCacheDirectory = DefaultModuleCacheDirectory();
    // good ol if/else config parsing, because the command line is at least simple for now
    // quick future upgrade would be a declarative lambda table, along with limits for safety
    for (size_t i = 0; i < arguments.size(); ++i)
    {
        const std::string_view argument = arguments[i];

        if (argument == "--output" || argument == "-o")
        {
            const CookResult<std::filesystem::path> outputPath = ReadPathArgument(arguments, i);
            if (!outputPath)
            {
                return std::unexpected(outputPath.error());
            }
            options.OutputPath = outputPath.value();
        }
        else if (argument == "--cache-dir")
        {
            const CookResult<std::filesystem::path> cacheDirectory = ReadPathArgument(arguments, i);
            if (!cacheDirectory)
            {
                return std::unexpected(cacheDirectory.error());
            }
            options.ModuleCacheDirectory = cacheDirectory.value();
        }
        else if (const SwitchFlag* flag = FindSwitchFlag(argument); flag != nullptr)
        {
            flag->Apply(options);
        }
        else if (argument.starts_with(k_StageDumpPrefix))
        {
            const CookResult<uint32_t> stageBits = ParseStageDumpArgument(argument);
            if (!stageBits)
            {
                return std::unexpected(stageBits.error());
            }
            options.DumpStageMask |= stageBits.value();
        }
        else if (argument.starts_with(k_OptimizationPrefix))
        {
            const CookResult<uint32_t> level = ParseOptimizationLevel(argument);
            if (!level)
            {
                return std::unexpected(level.error());
            }
            options.OptimizationLevel = level.value();
        }
        else if (argument.starts_with('-'))
        {
            return std::unexpected(CookError::UnknownArgument);
        }
        else
        {
            options.ModulePaths.emplace_back(argument);
        }
    }

    if (options.OutputPath.empty())
    {
        return std::unexpected(CookError::NoOutputSpecified);
    }

    if (options.ModulePaths.empty())
    {
        return std::unexpected(CookError::NoModulesSpecified);
    }

    return options;
}

std::string_view GetUsageText() noexcept
{
    return k_UsageText;
}

} // namespace lodestone
