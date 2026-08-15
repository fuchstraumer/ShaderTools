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
        "                 [--verify-deterministic] <module.slang>...\n"
        "  --output, -o    destination header path (required)\n"
        "  --O<level>      slang optimization level: 0-3, defaults to 0\n"
        "  --no-validate   skip cross-checking reflection against emitted WGSL\n"
        "  --quiet         suppress the per-variant reflection report\n"
        "  --cache-dir     directory for precompiled slang modules\n"
        "  --single-threaded disable multi-threaded entry point codegen\n"
        "  --no-dedupe     disable content deduplication\n"
        "  --verify-deterministic cook twice and compare all artifacts\n";

    constexpr std::string_view k_OptimizationPrefix = "--O";

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

    constexpr std::array<SwitchFlag, 5u> k_SwitchFlags{
        SwitchFlag{ .Name="--no-dedupe", .Apply=[](CookerOptions& options) noexcept { options.DedupeEnabled = false; } },
        SwitchFlag{ .Name="--verify-deterministic",
                    .Apply=[](CookerOptions& options) noexcept { options.VerifyDeterministic = true; } },
        SwitchFlag{ .Name="--no-validate",
                    .Apply=[](CookerOptions& options) noexcept { options.ValidateReflectionAgainstWgsl = false; } },
        SwitchFlag{ .Name="--quiet", .Apply=[](CookerOptions& options) noexcept { options.ReportReflection = false; } },
        SwitchFlag{ .Name="--single-threaded",
                    .Apply=[](CookerOptions& options) noexcept { options.MultithreadEntryPointCodegen = false; } }
    };

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
