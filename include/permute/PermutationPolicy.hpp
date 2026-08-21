#pragma once
#ifndef LODESTONE_PERMUTATION_POLICY_HPP
#define LODESTONE_PERMUTATION_POLICY_HPP
#include <cstdint>
#include <string_view>
#include <span>

namespace lodestone
{

/** One statement of what an axis is expected to do to an entry point's output.
 *
 * The cooker measures this and compares it against the declaration. A mismatch fails the cook, and
 * names the axis and the entry point. So a static branch that quadruples the variant count breaks the
 * build on the commit that adds it, instead of showing up later as a slow cook. */
struct ExpectedAxisInfluence
{
    std::string_view EntryPointName;
    std::string_view AxisName;
    /** True when the axis must not change this entry point's output. */
    bool IsInert{ false };
};

/** Limits a module's growth. `MaxVariants` of zero means no budget. */
struct ModulePolicy
{
    uint32_t MaxVariants{ 0u };
    std::span<const ExpectedAxisInfluence> ExpectedInfluence;
};

} // namespace lodestone

#endif // LODESTONE_PERMUTATION_POLICY_HPP
