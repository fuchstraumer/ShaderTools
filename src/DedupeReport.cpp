#include "DedupeReport.hpp"
#include <algorithm>
#include <format>
#include <print>

namespace velox::cooker
{

namespace
{

    const PermutationBinding* FindBinding(const PermutationAssignment& assignment,
                                          const PermutationAxis* axis) noexcept
    {
        for (const PermutationBinding& binding : assignment)
        {
            if (binding.first == axis)
            {
                return &binding;
            }
        }

        return nullptr;
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
                if (left.SourceIndices[entry_point_index] != right.SourceIndices[entry_point_index])
                {
                    return AxisInfluence::Active;
                }
            }
        }

        return foundPair ? AxisInfluence::Inert : AxisInfluence::Undetermined;
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
                const char marker = value == AxisInfluence::Active  ? 'x'
                                    : value == AxisInfluence::Inert ? '.'
                                                                    : '?';
                table += std::format("{:<24}", std::string(1u, marker));
            }
            table += "\n";
        }

        return table;
    }

    std::string EmitProvenance(const CookedModule& module)
    {
        std::string emitted;

        for (size_t entryPointIndex = 0u; entryPointIndex < module.EntryPoints.size();
             ++entryPointIndex)
        {
            const std::string& name = module.EntryPoints[entryPointIndex].Name;

            uint32_t artifactCount = 0u;
            std::vector<uint32_t> distinctSources;
            for (const LibraryVariant& variant : module.Variants)
            {
                ++artifactCount;
                const uint32_t sourceIndex = variant.SourceIndices[entryPointIndex];
                if (std::find(distinctSources.begin(), distinctSources.end(), sourceIndex) ==
                    distinctSources.end())
                {
                    distinctSources.push_back(sourceIndex);
                }
            }

            emitted += std::format("  {:<20} {} variants -> {} unique sources{}\n",
                                   name,
                                   artifactCount,
                                   distinctSources.size(),
                                   distinctSources.size() == artifactCount ? "   (no collapse)" : "");

            for (uint32_t sourceIndex : distinctSources)
            {
                uint32_t mapped = 0u;
                std::string firstDescription;
                for (const LibraryVariant& variant : module.Variants)
                {
                    if (variant.SourceIndices[entryPointIndex] != sourceIndex)
                    {
                        continue;
                    }

                    if (mapped == 0u)
                    {
                        firstDescription = variant.Description;
                    }
                    ++mapped;
                }

                if (mapped > 1u)
                {
                    emitted += std::format("      source #{} <- {} assignments, first [{}]\n",
                                           sourceIndex,
                                           mapped,
                                           firstDescription);
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

bool AllVariantsShareOneLayout(const CookedModule& module) noexcept
{
    return module.Layouts.size() <= 1u;
}

CookResult<void> EnforceModulePolicy(const CookedModule& module, const ModuleInfluence& influence)
{
    const ModulePolicy* policy = FindPolicyForModule(module.Name);
    if (policy == nullptr)
    {
        return {};
    }

    uint32_t violations = 0u;

    if (policy->MaxVariants != 0u && module.Variants.size() > policy->MaxVariants)
    {
        std::println(stderr,
                     "[shader_cooker] module {} expands to {} variants, over its budget of {}. Raise "
                     "the budget on purpose, or take an axis out.",
                     module.Name,
                     module.Variants.size(),
                     policy->MaxVariants);
        ++violations;
    }

    for (const ExpectedAxisInfluence& expected : policy->ExpectedInfluence)
    {
        const EntryPointInfluence* entry = nullptr;
        for (const EntryPointInfluence& candidate : influence.EntryPoints)
        {
            if (candidate.EntryPointName == expected.EntryPointName)
            {
                entry = &candidate;
                break;
            }
        }

        if (entry == nullptr)
        {
            std::println(stderr,
                         "[shader_cooker] module {} declares an expectation for entrypoint '{}', which "
                         "does not exist",
                         module.Name,
                         expected.EntryPointName);
            ++violations;
            continue;
        }

        size_t axisIndex = 0u;
        bool axisFound = false;
        for (size_t i = 0u; i < module.Space->size(); ++i)
        {
            if ((*module.Space)[i]->Name == expected.AxisName)
            {
                axisIndex = i;
                axisFound = true;
                break;
            }
        }

        if (!axisFound || axisIndex >= entry->Axes.size())
        {
            std::println(stderr,
                         "[shader_cooker] module {} declares an expectation for axis '{}', which is not "
                         "in its permutation space",
                         module.Name,
                         expected.AxisName);
            ++violations;
            continue;
        }

        const AxisInfluence measured = entry->Axes[axisIndex];
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
            ++violations;
        }
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

    for (const CookedModule& module : library.Modules)
    {
        const InternerStatistics& sourceStatistics = module.SourceInterner.Statistics();
        const InternerStatistics& layoutStatistics = module.LayoutInterner.Statistics();

        report += std::format("{}  {} variants x {} entrypoints = {} artifacts\n\n",
                              module.Name,
                              module.Variants.size(),
                              module.EntryPoints.size(),
                              sourceStatistics.ArtifactsSeen);

        report += EmitProvenance(module);
        report += "\n";

        report += std::format("  sources: {} -> {} unique\n",
                              sourceStatistics.ArtifactsSeen,
                              sourceStatistics.UniqueEntries);
        report += std::format("  layouts: {} -> {} unique{}\n",
                              layoutStatistics.ArtifactsSeen,
                              layoutStatistics.UniqueEntries,
                              AllVariantsShareOneLayout(module)
                                  ? "   (every permutation shares one layout)"
                                  : "");
        report += std::format("  dedup enabled: {}\n",
                              module.SourceInterner.IsEnabled() ? "yes" : "no");
        report += std::format("  hash function: {}\n", module.SourceInterner.HashName());
        report += std::format("  hash collisions resolved by byte compare: {}\n",
                              sourceStatistics.HashCollisions + layoutStatistics.HashCollisions);
        report += std::format("  byte comparisons forced by a hash hit: {}\n",
                              sourceStatistics.ByteComparisons + layoutStatistics.ByteComparisons);
        report += "  normalization passes active: (none)\n\n";

        const ModuleInfluence influence = ComputeAxisInfluence(module);
        report += EmitInfluenceTable(module, influence);
        report += "\n";
    }

    return report;
}

} // namespace velox::cooker
