#include "CookerOptions.hpp"
#include <charconv>

namespace velox::cooker
{

namespace
{

    constexpr std::string_view k_UsageText =
        "Usage: OceanShaderCompiler --output <header.hpp> [--O<level>] [--no-validate] [--quiet]\n"
        "                           [--cache-dir <path>] [--single-threaded] <module.slang>...\n"
        "  --output, -o    destination header path (required)\n"
        "  --O<level>      slang optimization level: 0-3, defaults to 0\n"
        "  --no-validate   skip cross-checking reflection against emitted WGSL\n"
        "  --quiet         suppress the per-variant reflection report\n"
        "  --cache-dir     directory for precompiled slang modules\n";

    constexpr std::string_view k_OptimizationPrefix = "--O";

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

    std::filesystem::path DefaultModuleCacheDirectory()
    {
        return std::filesystem::temp_directory_path() / "VeloxShaderCooker";
    }

} // namespace

CookResult<CookerOptions> ParseCommandLine(std::span<const std::string_view> arguments)
{
    CookerOptions options;
    options.ModuleCacheDirectory = DefaultModuleCacheDirectory();

    for (size_t i = 0; i < arguments.size(); ++i)
    {
        const std::string_view argument = arguments[i];

        if (argument == "--output" || argument == "-o")
        {
            if (i + 1u >= arguments.size())
            {
                return std::unexpected(CookError::MalformedArgument);
            }
            options.OutputPath = arguments[++i];
        }
        else if (argument == "--cache-dir")
        {
            if (i + 1u >= arguments.size())
            {
                return std::unexpected(CookError::MalformedArgument);
            }
            options.ModuleCacheDirectory = arguments[++i];
        }
        else if (argument == "--no-validate")
        {
            options.ValidateReflectionAgainstWgsl = false;
        }
        else if (argument == "--quiet")
        {
            options.ReportReflection = false;
        }
        else if (argument == "--single-threaded")
        {
            options.MultithreadEntryPointCodegen = false;
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

} // namespace velox::cooker
