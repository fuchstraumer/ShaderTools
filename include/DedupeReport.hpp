#pragma once
#ifndef VELOX_SHADER_COOKER_DEDUPE_REPORT_HPP
#define VELOX_SHADER_COOKER_DEDUPE_REPORT_HPP
#include "CookedLibrary.hpp"
#include <string>
#include <vector>

/**
 * What the dedup pass did, written as a build artifact rather than a log line that scrolls past.
 *
 * The influence matrix is the part worth reading. For each entry point and each axis, it groups the
 * variants by every other axis, then checks whether the interned source index stays the same as that
 * axis changes. An axis that never changes the output is inert, and it multiplies the variant count
 * for nothing.
 *
 * The comparison costs one integer check for each pair, because interning already replaced the text
 * with an index. The result turns combinatorial growth into a number you read before the space gets
 * large, instead of a build time you find afterwards.
 */
namespace velox::cooker
{

enum class AxisInfluence : uint8_t
{
    Invalid = 0,
    /** The axis never changes this entry point's output. */
    Inert,
    /** The axis changes this entry point's output. */
    Active,
    /** Too few variants to decide. One axis value cannot show a difference. */
    Undetermined,
};

struct EntryPointInfluence
{
    std::string EntryPointName;
    /** Runs parallel to the module's permutation space. */
    std::vector<AxisInfluence> Axes;
};

struct ModuleInfluence
{
    std::string ModuleName;
    std::vector<EntryPointInfluence> EntryPoints;
};

ModuleInfluence ComputeAxisInfluence(const CookedModule& module);

/** True when every variant of the module produced the same binding layout. The graph can then hold
 * one layout for each module instead of one for each variant. */
bool AllVariantsShareOneLayout(const CookedModule& module) noexcept;

/** Compares the measured influence against what the module declared, and checks the variant budget.
 * A mismatch fails the cook and names the entry point and the axis. */
CookResult<void> EnforceModulePolicy(const CookedModule& module, const ModuleInfluence& influence);

std::string GenerateDedupeReport(const CookedLibrary& library);

std::string_view ToString(AxisInfluence influence) noexcept;

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_DEDUPE_REPORT_HPP
