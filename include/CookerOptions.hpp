#pragma once
#ifndef VELOX_SHADER_COOKER_OPTIONS_HPP
#define VELOX_SHADER_COOKER_OPTIONS_HPP
#include "CookerErrors.hpp"
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace velox::cooker
{

struct CookerOptions
{
    std::filesystem::path OutputPath;
    std::filesystem::path ModuleCacheDirectory;
    std::vector<std::filesystem::path> ModulePaths;
    uint32_t OptimizationLevel{ 0u };
    bool ValidateReflectionAgainstWgsl{ true };
    bool ReportReflection{ true };
    bool MultithreadEntryPointCodegen{ true };
};

CookResult<CookerOptions> ParseCommandLine(std::span<const std::string_view> arguments);
std::string_view GetUsageText() noexcept;

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_OPTIONS_HPP
