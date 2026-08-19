#include "DedupeReport.hpp"
#include "ContentInterner.hpp"
#include "CookedLibrary.hpp"
#include "CookerErrors.hpp"
#include "PermutationSpace.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lodestone
{

namespace
{

    const PermutationBinding* FindBinding(const PermutationAssignment& assignment,
                                          const PermutationAxis* axis) noexcept
    {
        auto foundIter = std::ranges::find_if(assignment,
                                              [axis](const PermutationBinding& binding)
                                              {
                                                  return binding.first == axis;
                                              });
        return foundIter != assignment.end() ? &(*foundIter) : nullptr;
    }

    /** True when two assignments agree on every axis except the one under test. */
    bool DiffersOnlyInAxis(const PermutationAssignment& left,
                           const PermutationAssignment& right,
                           const PermutationAxis* axis) noexcept
    {
        for (const PermutationBinding& binding : left)
        {
            if (binding.first == axis)
            {
                continue;
            }

            const PermutationBinding* other = FindBinding(right, binding.first);
            if (other == nullptr || other->second != binding.second)
            {
                return false;
            }
        }

        const PermutationBinding* leftValue = FindBinding(left, axis);
        const PermutationBinding* rightValue = FindBinding(right, axis);
        if (leftValue == nullptr || rightValue == nullptr)
        {
            return false;
        }

        return leftValue->second != rightValue->second;
    }

    /** Compares the text, never the index.
     *
     * An index is what the interner assigned, not what the shader says. `--no-dedupe` gives every
     * artifact its own index, so an index comparison then reports every axis Active and every module
     * with a declared inert axis fails its policy. The escape hatch is the A/B control arm, so a
     * measurement that only works in one arm is not a measurement.
     *
     * With dedup on the two comparisons agree by construction: equal text takes one entry, so equal
     * text and equal index are the same fact. */
    bool EntryPointTextIsEqual(const CookedModule& module,
                               const LibraryVariant& left,
                               const LibraryVariant& right,
                               size_t entry_point_index) noexcept
    {
        return ResolveSource(module, left, entry_point_index) ==
               ResolveSource(module, right, entry_point_index);
    }

    AxisInfluence InfluenceOfAxis(const CookedModule& module,
                                  size_t entry_point_index,
                                  const PermutationAxis* axis)
    {
        bool foundPair = false;

        for (size_t i = 0u; i < module.Variants.size(); ++i)
        {
            for (size_t j = i + 1u; j < module.Variants.size(); ++j)
            {
                const LibraryVariant& left = module.Variants[i];
                const LibraryVariant& right = module.Variants[j];

                if (!DiffersOnlyInAxis(left.Canonical, right.Canonical, axis))
                {
                    continue;
                }

                foundPair = true;
                if (!EntryPointTextIsEqual(module, left, right, entry_point_index))
                {
                    return AxisInfluence::Active;
                }
            }
        }

        return foundPair ? AxisInfluence::Inert : AxisInfluence::Undetermined;
    }

    /** The single character the influence table prints for one axis. The heading above the table
     * states what each one means, so the two must stay together. */
    char InfluenceMarker(AxisInfluence influence) noexcept
    {
        switch (influence)
        {
        case AxisInfluence::Active:
            return 'x';
        case AxisInfluence::Inert:
            return '.';
        default:
            return '?';
        }
    }

    std::string EmitInfluenceTable(const CookedModule& module, const ModuleInfluence& influence)
    {
        std::string table = "  axis influence (x = changes output, . = inert, ? = undetermined)\n\n";

        size_t nameWidth = 16u;
        for (const EntryPointInfluence& entry : influence.EntryPoints)
        {
            nameWidth = std::max(nameWidth, entry.EntryPointName.size() + 2u);
        }

        table += std::format("  {:<{}}", "", nameWidth);
        for (const PermutationAxis* axis : *module.Space)
        {
            table += std::format("{:<24}", axis->Name);
        }
        table += "\n";

        for (const EntryPointInfluence& entry : influence.EntryPoints)
        {
            table += std::format("  {:<{}}", entry.EntryPointName, nameWidth);
            for (AxisInfluence value : entry.Axes)
            {
                table += std::format("{:<24}", std::string(1u, InfluenceMarker(value)));
            }
            table += "\n";
        }

        return table;
    }

    /** How many variants mapped onto one interned source, and which one arrived first.
     *
     * The order of first arrival decides the report text, so this keeps the sources in that order
     * rather than in index order. */
    struct SourceCollapse
    {
        uint32_t SourceIndex{ 0u };
        uint32_t MappedCount{ 0u };
        std::string_view FirstDescription;
    };

    /** One pass over the variants. The earlier form searched the whole variant list again for each
     * distinct source, which is quadratic and gives the same answer. */
    std::vector<SourceCollapse> CollectSourceCollapses(const CookedModule& module, size_t entry_point_index)
    {
        std::vector<SourceCollapse> collapses;

        for (const LibraryVariant& variant : module.Variants)
        {
            const uint32_t sourceIndex = variant.SourceIndices[entry_point_index];

            const std::vector<SourceCollapse>::iterator found =
                std::ranges::find(collapses, sourceIndex, &SourceCollapse::SourceIndex);

            if (found != collapses.end())
            {
                ++found->MappedCount;
                continue;
            }

            collapses.push_back(SourceCollapse{
                .SourceIndex = sourceIndex, .MappedCount = 1u, .FirstDescription = variant.Description });
        }

        return collapses;
    }

    std::string EmitProvenance(const CookedModule& module)
    {
        std::string emitted;

        for (size_t entryPointIndex = 0u; entryPointIndex < module.EntryPoints.size(); ++entryPointIndex)
        {
            const std::string& name = module.EntryPoints[entryPointIndex].Name;
            const std::vector<SourceCollapse> collapses = CollectSourceCollapses(module, entryPointIndex);
            const size_t artifactCount = module.Variants.size();

            emitted += std::format("  {:<20} {} variants -> {} unique sources{}\n",
                                   name,
                                   artifactCount,
                                   collapses.size(),
                                   collapses.size() == artifactCount ? "   (no collapse)" : "");

            for (const SourceCollapse& collapse : collapses)
            {
                if (collapse.MappedCount > 1u)
                {
                    emitted += std::format("      source #{} <- {} assignments, first [{}]\n",
                                           collapse.SourceIndex,
                                           collapse.MappedCount,
                                           collapse.FirstDescription);
                }
            }
        }

        return emitted;
    }

} // namespace

std::string_view ToString(AxisInfluence influence) noexcept
{
    switch (influence)
    {
    case AxisInfluence::Inert:
        return "Inert";
    case AxisInfluence::Active:
        return "Active";
    case AxisInfluence::Undetermined:
        return "Undetermined";
    case AxisInfluence::Invalid:
        return "Invalid";
    }

    return "Invalid";
}

ModuleInfluence ComputeAxisInfluence(const CookedModule& module)
{
    ModuleInfluence influence;
    influence.ModuleName = module.Name;

    if (module.Space == nullptr)
    {
        return influence;
    }

    influence.EntryPoints.reserve(module.EntryPoints.size());

    for (size_t entryPointIndex = 0u; entryPointIndex < module.EntryPoints.size(); ++entryPointIndex)
    {
        EntryPointInfluence entry;
        entry.EntryPointName = module.EntryPoints[entryPointIndex].Name;
        entry.Axes.reserve(module.Space->size());

        for (const PermutationAxis* axis : *module.Space)
        {
            entry.Axes.push_back(InfluenceOfAxis(module, entryPointIndex, axis));
        }

        influence.EntryPoints.push_back(std::move(entry));
    }

    return influence;
}

// I'm keeping this around in case we want to use it, but this isn't really a useful
// query in 99.9% of cases: it was an interesting special case for our first ever
// test content since we could use this to verify collapsing worked as expected.
bool AllVariantsShareOneLayout(const CookedModule& module)
{
    bool seenOne = false;
    ShaderLayout first;

    for (const LibraryVariant& variant : module.Variants)
    {
        for (size_t i = 0u; i < variant.VisibilityIndices.size(); ++i)
        {
            ShaderLayout layout = ResolveLayout(module, variant, i);

            if (!seenOne)
            {
                first = std::move(layout);
                seenOne = true;
            }
            else if (layout != first)
            {
                return false;
            }
        }
    }

    return true;
}

namespace
{

    const EntryPointInfluence* FindEntryPointInfluence(const ModuleInfluence& influence,
                                                       std::string_view entry_point_name) noexcept
    {
        for (const EntryPointInfluence& candidate : influence.EntryPoints)
        {
            if (candidate.EntryPointName == entry_point_name)
            {
                return &candidate;
            }
        }

        return nullptr;
    }

    /** Position of an axis in the space. The influence vector runs parallel to it. */
    std::optional<size_t> FindAxisIndex(const PermutationSpace& space, std::string_view axis_name) noexcept
    {
        for (size_t i = 0u; i < space.size(); ++i)
        {
            if (space[i]->Name == axis_name)
            {
                return i;
            }
        }

        return std::nullopt;
    }

    /** One declared expectation, checked against what the cook measured. Returns the violation count,
     * which is zero or one. */
    uint32_t CheckExpectedInfluence(const CookedModule& module,
                                    const ModuleInfluence& influence,
                                    const ExpectedAxisInfluence& expected)
    {
        const EntryPointInfluence* entry = FindEntryPointInfluence(influence, expected.EntryPointName);
        if (entry == nullptr)
        {
            std::println(stderr,
                         "[shader_cooker] module {} declares an expectation for entrypoint '{}', which "
                         "does not exist",
                         module.Name,
                         expected.EntryPointName);
            return 1u;
        }

        const std::optional<size_t> axisIndex = FindAxisIndex(*module.Space, expected.AxisName);
        if (!axisIndex.has_value() || axisIndex.value() >= entry->Axes.size())
        {
            std::println(stderr,
                         "[shader_cooker] module {} declares an expectation for axis '{}', which is not "
                         "in its permutation space",
                         module.Name,
                         expected.AxisName);
            return 1u;
        }

        const AxisInfluence measured = entry->Axes[axisIndex.value()];
        const bool measuredInert = measured == AxisInfluence::Inert;

        if (measured != AxisInfluence::Undetermined && measuredInert != expected.IsInert)
        {
            std::println(stderr,
                         "[shader_cooker] INFLUENCE CHANGED: axis '{}' is {} for {}, but the module "
                         "declares it {}. The permutation space now costs something different.",
                         expected.AxisName,
                         ToString(measured),
                         expected.EntryPointName,
                         expected.IsInert ? "Inert" : "Active");
            return 1u;
        }

        return 0u;
    }

    uint32_t CheckVariantBudget(const CookedModule& module, const ModulePolicy& policy)
    {
        if (policy.MaxVariants == 0u || module.Variants.size() <= policy.MaxVariants)
        {
            return 0u;
        }

        std::println(stderr,
                     "[shader_cooker] module {} expands to {} variants, over its budget of {}. Raise "
                     "the budget on purpose, or take an axis out.",
                     module.Name,
                     module.Variants.size(),
                     policy.MaxVariants);
        return 1u;
    }

} // namespace

CookResult<void> EnforceModulePolicy(const CookedModule& module, const ModuleInfluence& influence)
{
    const ModulePolicy* policy = FindPolicyForModule(module.Name);
    if (policy == nullptr)
    {
        return {};
    }

    uint32_t violations = CheckVariantBudget(module, *policy);

    for (const ExpectedAxisInfluence& expected : policy->ExpectedInfluence)
    {
        violations += CheckExpectedInfluence(module, influence, expected);
    }

    if (violations != 0u)
    {
        return std::unexpected(CookError::ModulePolicyViolated);
    }

    return {};
}

std::string GenerateDedupeReport(const CookedLibrary& library)
{
    std::string report;
    report.reserve(1u << 13);

    report += "Shader cooker dedup report\n";
    report += "Generated by tools/shader_cooker. Do not edit by hand.\n\n";
    report += "Note: The dedupe ratio indicates how many artifacts were seen for each unique entry. A higher ratio means more effective deduplication.\n\n";

    for (const CookedModule& module : library.Modules)
    {
        const InternerStatistics& sourceStatistics = module.SourceTable.Interning;

        report += std::format("{}  {} variants x {} entrypoints = {} artifacts\n\n",
                              module.Name,
                              module.Variants.size(),
                              module.EntryPoints.size(),
                              sourceStatistics.ArtifactsSeen);

        report += EmitProvenance(module);
        report += "\n";

        // One line for each table. Placement, footprint, and visibility collapse at different rates,
        // and one number for all three would hide which one grows.
        const std::array<std::pair<std::string_view, const TableStatistics*>, 5u> tables{
            std::pair{ std::string_view{ "sources" }, &module.SourceTable },
            std::pair{ std::string_view{ "resources" }, &module.ResourceTable },
            std::pair{ std::string_view{ "resource lists" }, &module.ResourceListTable },
            std::pair{ std::string_view{ "footprints" }, &module.FootprintListTable },
            std::pair{ std::string_view{ "visibility" }, &module.VisibilityTable }
        };

        uint32_t collisions = 0u;
        uint32_t comparisons = 0u;

        for (const auto& [name, table] : tables)
        {
            const float dedupeRatio = static_cast<float>(table->Interning.ArtifactsSeen) /
                                      static_cast<float>(table->Interning.UniqueEntries);
            report += std::format("  {}: Artifacts seen: {} -> Unique Entries: {} (Dedupe Ratio: {:.2f}:1)\n",
                                  name,
                                  table->Interning.ArtifactsSeen,
                                  table->Interning.UniqueEntries,
                                  dedupeRatio);
            collisions += table->Interning.HashCollisions;
            comparisons += table->Interning.ByteComparisons;
        }

        report += std::format("  dedup enabled: {}\n", module.SourceTable.DedupeEnabled ? "yes" : "no");
        report += std::format("  hash function: {}\n", module.SourceTable.HashName);
        report += std::format("  hash collisions resolved by byte compare: {}\n", collisions);
        report += std::format("  byte comparisons forced by a hash hit: {}\n", comparisons);
        report += "  normalization passes active: (none)\n\n";

        const ModuleInfluence influence = ComputeAxisInfluence(module);
        report += EmitInfluenceTable(module, influence);
        report += "\n";
    }

    return report;
}

} // namespace lodestone
