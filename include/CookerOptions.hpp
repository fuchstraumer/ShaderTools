#pragma once
#ifndef LODESTONE_SHADER_COOKER_OPTIONS_HPP
#define LODESTONE_SHADER_COOKER_OPTIONS_HPP
#include "CookerErrors.hpp"
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace lodestone
{

struct CookerOptions
{
    std::filesystem::path OutputPath;
    std::filesystem::path ModuleCacheDirectory;
    std::vector<std::filesystem::path> ModulePaths;
    uint32_t OptimizationLevel{ 0u };
    bool ValidateReflectionAgainstWgsl{ true };
    bool ReportReflection{ false };
    bool MultithreadEntryPointCodegen{ true };
    /** Turns off content dedup. Output stays correct, and every artifact takes its own index. This
     * is the control arm of the A/B check, and the way out if dedup ever needs to be untangled. */
    bool DedupeEnabled{ true };
    /** Cooks twice into memory and compares. Catches an unordered container's iteration order when
     * it reaches the emitted output. */
    bool VerifyDeterministic{ false };
};

CookResult<CookerOptions> ParseCommandLine(std::span<const std::string_view> arguments);
std::string_view GetUsageText() noexcept;

} // namespace lodestone

#endif // !LODESTONE_SHADER_COOKER_OPTIONS_HPP
