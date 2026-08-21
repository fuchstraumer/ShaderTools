#include "permute/PermutationSpace.hpp"
#include "CookerErrors.hpp"
#include "permute/PermutationAssignment.hpp"
#include "permute/PermutationAxis.hpp"
#include "permute/PermutationPolicy.hpp"
#include "permute/PermutationValue.hpp"
#include "permute/SizeExpression.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <format>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lodestone
{

namespace
{

    // Axis order is the declaration order, and `ParentIndex` is a position in this list.
    // IFFT_WAVE_SIZE names index 1, which is IFFT_USE_WAVE_OPS.
    const PermutationSpace k_OceanFftSpace{
        "OceanFft",
        { PermutationAxis{ "IFFT_SIZE",
                           { PermutationValue{ 128u },
                             PermutationValue{ 256u },
                             PermutationValue{ 512u },
                             PermutationValue{ 1024u },
                             PermutationValue{ 2048u },
                             PermutationValue{ 4096u },
                             PermutationValue{ 8192u } },
                           PermutationAxis::k_NoParent,
                           PermutationValue{} },
          PermutationAxis{ "IFFT_USE_WAVE_OPS",
                           { PermutationValue{ false }, PermutationValue{ true } },
                           PermutationAxis::k_NoParent,
                           PermutationValue{} },
          PermutationAxis{ "IFFT_WAVE_SIZE",
                           { PermutationValue{ 16u },
                             PermutationValue{ 32u },
                             PermutationValue{ 64u },
                             PermutationValue{ 128u } },
                           1,
                           PermutationValue{ true } } } };

    const PermutationSpace k_EmptySpace{ "", {} };

    // IfftPermuteCS reorders data and never reads a wave-op symbol, so both wave axes must stay inert
    // for it. If that ever changes, the entry point started paying for a permutation it does not use.
    const std::array<ExpectedAxisInfluence, 2> k_OceanFftExpectedInfluence{
        ExpectedAxisInfluence{
            .EntryPointName = "IfftPermuteCS", .AxisName = "IFFT_USE_WAVE_OPS", .IsInert = true },
        ExpectedAxisInfluence{
            .EntryPointName = "IfftPermuteCS", .AxisName = "IFFT_WAVE_SIZE", .IsInert = true }
    };

    const ModulePolicy k_OceanFftPolicy{ .MaxVariants = 64u,
                                         .ExpectedInfluence = k_OceanFftExpectedInfluence };
    const ModulePolicy k_EmptyPolicy{};

    struct ModuleSpaceEntry
    {
        std::string_view ModuleName;
        const PermutationSpace* Space;
        const ModulePolicy* Policy;
    };

    const std::array<ModuleSpaceEntry, 1> k_ModuleSpaces{ ModuleSpaceEntry{
        .ModuleName = "OceanFft", .Space = &k_OceanFftSpace, .Policy = &k_OceanFftPolicy } };

    const PermutationBinding* FindBindingForAxis(const PermutationAssignment& assignment,
                                                 const PermutationAxis* axis) noexcept
    {
        auto bindingIter = std::ranges::find_if(assignment,
                                                [axis](const PermutationBinding& binding)
                                                {
                                                    return binding.Axis == axis;
                                                });
        if (bindingIter != assignment.end())
        {
            return std::to_address(bindingIter);
        }
        return nullptr;
    }

    bool IsIdentifierCharacter(char character) noexcept
    {
        return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
    }

    /** True when `name` appears as a whole identifier on a line that also declares `extern`. Text
     * matching rather than reflection: link-time constants never reach a program layout, and the
     * failure this guards against is a typo in the axis name, which the source text shows directly. */
    bool ContainsExternDeclarationOf(std::string_view source, std::string_view name)
    {
        constexpr std::string_view k_ExternKeyword{ "extern" };

        size_t lineStart = 0u;
        while (lineStart < source.size())
        {
            size_t lineEnd = source.find('\n', lineStart);
            if (lineEnd == std::string_view::npos)
            {
                lineEnd = source.size();
            }

            const std::string_view line = source.substr(lineStart, lineEnd - lineStart);
            lineStart = lineEnd + 1u;

            if (!line.contains(k_ExternKeyword))
            {
                continue;
            }

            size_t searchFrom = 0u;
            while (true)
            {
                const size_t found = line.find(name, searchFrom);
                if (found == std::string_view::npos)
                {
                    break;
                }

                const bool startIsBoundary = found == 0u || !IsIdentifierCharacter(line[found - 1u]);
                const size_t afterIndex = found + name.size();
                const bool endIsBoundary =
                    afterIndex >= line.size() || !IsIdentifierCharacter(line[afterIndex]);

                if (startIsBoundary && endIsBoundary)
                {
                    return true;
                }

                searchFrom = found + 1u;
            }
        }

        return false;
    }

    /** Collects the identifier declared by every `extern ... <name> =` line. Each one is a knob the
     * cooker is expected to drive; one without an axis silently keeps its default forever. */
    std::vector<std::string_view> CollectExternConstantNames(std::string_view source)
    {
        constexpr static std::string_view k_ExternKeyword{ "extern" };
        std::vector<std::string_view> names;

        size_t lineStart = 0u;
        while (lineStart < source.size())
        {
            size_t lineEnd = source.find('\n', lineStart);
            if (lineEnd == std::string_view::npos)
            {
                lineEnd = source.size();
            }

            const std::string_view line = source.substr(lineStart, lineEnd - lineStart);
            lineStart = lineEnd + 1u;

            if (!line.contains(k_ExternKeyword))
            {
                continue;
            }

            const size_t assignIndex = line.find('=');
            if (assignIndex == std::string_view::npos)
            {
                continue;
            }

            size_t nameEnd = assignIndex;
            while (nameEnd > 0u && !IsIdentifierCharacter(line[nameEnd - 1u]))
            {
                --nameEnd;
            }

            size_t nameStart = nameEnd;
            while (nameStart > 0u && IsIdentifierCharacter(line[nameStart - 1u]))
            {
                --nameStart;
            }

            if (nameEnd > nameStart)
            {
                names.push_back(line.substr(nameStart, nameEnd - nameStart));
            }
        }

        return names;
    }

    /** The initializer text of every `extern ... <name> = <value>;` line, paired with the name. */
    std::vector<std::pair<std::string_view, std::string_view>> CollectExternConstantDefinitions(
        std::string_view source)
    {
        constexpr static std::string_view k_ExternKeyword{ "extern" };
        std::vector<std::pair<std::string_view, std::string_view>> definitions;

        size_t lineStart = 0u;
        while (lineStart < source.size())
        {
            size_t lineEnd = source.find('\n', lineStart);
            if (lineEnd == std::string_view::npos)
            {
                lineEnd = source.size();
            }

            const std::string_view line = source.substr(lineStart, lineEnd - lineStart);
            lineStart = lineEnd + 1u;

            if (!line.contains(k_ExternKeyword))
            {
                continue;
            }

            const size_t assignIndex = line.find('=');
            if (assignIndex == std::string_view::npos)
            {
                continue;
            }

            size_t nameEnd = assignIndex;
            while (nameEnd > 0u && !IsIdentifierCharacter(line[nameEnd - 1u]))
            {
                --nameEnd;
            }

            size_t nameStart = nameEnd;
            while (nameStart > 0u && IsIdentifierCharacter(line[nameStart - 1u]))
            {
                --nameStart;
            }

            if (nameEnd <= nameStart)
            {
                continue;
            }

            const size_t valueStart = assignIndex + 1u;
            size_t valueEnd = line.find(';', valueStart);
            if (valueEnd == std::string_view::npos)
            {
                valueEnd = line.size();
            }

            definitions.emplace_back(line.substr(nameStart, nameEnd - nameStart),
                                     line.substr(valueStart, valueEnd - valueStart));
        }

        return definitions;
    }

    std::string_view TrimWhitespace(std::string_view text) noexcept
    {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
        {
            text.remove_prefix(1u);
        }

        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
        {
            text.remove_suffix(1u);
        }

        return text;
    }

    /** Lets a later extern's default read an earlier one, which is how shaders usually derive them. */
    std::vector<SizeSymbol> AsSizeSymbols(const std::vector<ExternConstantDefault>& defaults)
    {
        std::vector<SizeSymbol> symbols;
        symbols.reserve(defaults.size());

        for (const ExternConstantDefault& entry : defaults)
        {
            symbols.push_back(SizeSymbol{ .Name = entry.Name, .Value = entry.Value });
        }

        return symbols;
    }

    bool SpaceDrivesAxisNamed(const PermutationSpace& space, std::string_view name) noexcept
    {
        return std::ranges::any_of(space.Axes(),
                                   [name](const PermutationAxis& axis)
                                   {
                                       return axis.Name == name;
                                   });
    }

    [[nodiscard]] CookError VerifyVariantIndicesAreUnique(const std::vector<VariantDescriptor>& variants)
    {
        auto firstDuplicateIter =
            std::ranges::adjacent_find(variants,
                                       [](const VariantDescriptor& lhs, const VariantDescriptor& rhs)
                                       {
                                           return lhs.Index == rhs.Index;
                                       });

        if (firstDuplicateIter != variants.end()) [[unlikely]]
        {
            // get variant that caused the collision
            const VariantDescriptor& duplicate = *firstDuplicateIter;
            std::println(stderr,
                         "[shader_cooker] two variants share index {}: [{}] collides. The mixed-radix "
                         "encoding and the enumerated set disagree.",
                         duplicate.Index,
                         DescribeAssignment(duplicate.Canonical));
            return CookError::PermutationVariantIndexCollision;
        }
        else [[likely]]
        {
            return CookError::Success;
        }
    }

} // namespace

PermutationSpace::PermutationSpace(std::string _name, std::span<const PermutationAxis> _axes) noexcept
    : name{ std::move(_name) },
      axes{ _axes.begin(), _axes.end() }
{
}

PermutationSpace::PermutationSpace(std::string _name, std::initializer_list<PermutationAxis> _axes) noexcept
    : name{ std::move(_name) },
      axes{ _axes }
{
}

std::string_view PermutationSpace::Name() const noexcept
{
    return name;
}

std::span<const PermutationAxis> PermutationSpace::Axes() const noexcept
{
    return axes;
}

std::size_t PermutationSpace::AxisCount() const noexcept
{
    return axes.size();
}

bool PermutationSpace::IsEmpty() const noexcept
{
    return axes.empty();
}

const PermutationAxis* PermutationSpace::ParentOf(const PermutationAxis& axis) const noexcept
{
    if (!axis.HasParent())
    {
        return nullptr;
    }

    return &axes[static_cast<size_t>(axis.ParentIndex)];
}

// perform CCSP with classic backtracking, but skip any axis whose parent is not active
// this is a somewhat embarassing amount of commenting for me, but I have not done constraint satisfaction
// formally *ever* before, and I want to make sure I understand it. these are notes for me. i am not a learned
// woman
CookResult<std::vector<PermutationAssignment>> PermutationSpace::EnumerateActiveCombinations() const
{
    // A module with no registered space enumerates to the one empty assignment, so there is no first
    // axis to size against. `partials` is replaced by `expanded` on every pass anyway.
    std::vector<PermutationAssignment> partials{ PermutationAssignment{} };
    for (const PermutationAxis& axis : axes)
    {
        // Despite having to do recursive work here, we can at least reserve the right amount of space. I
        // guess.
        std::vector<PermutationAssignment> expanded;
        expanded.reserve(partials.size() * static_cast<size_t>(axis.NumValues()));
        // For the current axis, we need to traverse every partial assignment (incomplete combination) we have
        // thus far and expand/evaluate it for the current axis. This is a breadth-first search of the
        // combination space, and we will continue to expand the partials until we have a complete assignment
        // for every axis in the space.
        for (const PermutationAssignment& partial : partials)
        {
            // If an axis has a parent, we must check that parent to know whether to expand this current axis
            // If there is no parent, we proceed to just expand the axis as normal
            const PermutationAxis* parentAxis = ParentOf(axis);
            if (parentAxis != nullptr)
            {
                // Read the current list of active axes to see if the parent is active, and retrieve
                // it if it is. If the parent is not active (present), there is an axis declaration
                // order error.
                const PermutationBinding* parentBinding = FindBindingForAxis(partial, parentAxis);
                if (parentBinding == nullptr)
                {
                    // todo-ship: This is a user error, and should be evaluated during initial load when we're
                    // already traversing permutations to check for undriven values, etc. Flatten this
                    // calltree to use less Results
                    return std::unexpected(CookError::PermutationParentAxisMissing);
                }
                // If the parent is active, but not set to the required value, close partial off
                // for this current partial (where parent axis was evaluated to the wrong value)
                if (parentBinding->Value != axis.RequiredParentValue)
                {
                    expanded.push_back(partial);
                    continue;
                }
            }

            // Expand the current axis, evaluating/instantiating it for each of it's values
            // We store the axis (the abstract half) and the *value* (the concrete half). This
            // defines a *Binding* or unique instantiation of the axis for this current partial.
            // (thus a binding is just {abstract [axis*], concrete [value]})
            for (const PermutationValue& value : axis.GetValues())
            {
                // at each depth, we take the current partial as our starting point (as that's how
                // breadth-first constraint satisfaction like this works best for our data)
                PermutationAssignment next = partial;
                next.push_back(PermutationBinding{ .Axis = &axis, .Value = value });
                // note: we need expanded separate as we are using partials as the source of truth for
                // the current depth, and we don't want to modify it while iterating. the overwrite
                // has to come at the end
                expanded.push_back(std::move(next));
            }
        }

        // now that we're done reading partials, we can overwrite it with the expanded set of partials at the
        // current depth... to use at the next depth.
        partials = std::move(expanded);
    }

    return partials;
}

// Canonicalization is another expansion: for every axis in the space, we need to find the concrete
// value of it bound in *this* assignment. If the axis is not present in the assignment, we will
// retrieve the default value (the first value) of the axis. This equalizes each assignment to the
// same length, and allows us to compute a unique index for each assignment.
CanonicalAssignment PermutationSpace::CanonicalizeAssignment(const PermutationAssignment& assignment) const
{
    PermutationAssignment canonical;
    canonical.reserve(axes.size());
    // So, as mentioned above: step through each axis.
    for (const PermutationAxis& axis : axes)
    {
        // Get the binding (concrete instantiation) of the current axis for *this* assignment.
        const PermutationBinding* binding = FindBindingForAxis(assignment, &axis);
        // Now check: did we fail to find the binding? That means it was folded out of the assignment,
        // because one of it's dependent axes values was not set as needed. Thus, default value assigned.
        const PermutationValue value = binding != nullptr ? binding->Value : axis.GetDefault();
        canonical.push_back(PermutationBinding{ .Axis = &axis, .Value = value });
    }
    // And bam, the canonical assignment is just a fully "concrete" instance of the *actual* active assignment
    return CanonicalAssignment{ std::move(canonical) };
}

int32_t PermutationSpace::ComputeVariantIndex(const CanonicalAssignment& canonical) const
{
    std::ptrdiff_t index = 0;

    for (size_t i = 0; i < axes.size(); ++i)
    {
        // in canonical, the i-th element corresponds to the i-th axis in the space.
        const PermutationValue& value = canonical[i].Value;
        const std::span<const PermutationValue> values = axes[i].GetValues();
        const auto found = std::ranges::find(values, value);
        const std::ptrdiff_t valueIndex = std::distance(values.begin(), found);
        index = (index * std::ssize(values)) + valueIndex;
    }

    return static_cast<int32_t>(index);
}

int32_t PermutationSpace::ComputeVariantSpaceSize() const noexcept
{
    int32_t size = 1;

    for (const PermutationAxis& axis : axes)
    {
        size *= static_cast<int32_t>(axis.NumValues());
    }

    return size;
}

CookResult<VariantSet> PermutationSpace::EnumerateVariants() const
{
    CookResult<std::vector<PermutationAssignment>> enumerateActiveResult = EnumerateActiveCombinations();
    if (!enumerateActiveResult)
    {
        return std::unexpected(enumerateActiveResult.error());
    }

    std::vector<PermutationAssignment> active{ std::move(enumerateActiveResult.value()) };

    VariantSet variantSet;
    variantSet.Space = this;
    variantSet.SpaceSize = ComputeVariantSpaceSize();
    variantSet.Variants.reserve(active.size());

    for (PermutationAssignment& assignment : active)
    {
        CanonicalAssignment canonical = CanonicalizeAssignment(assignment);
        const int32_t index = ComputeVariantIndex(canonical);
        variantSet.Variants.emplace_back(std::move(assignment), std::move(canonical), index);
    }

    // sort first, because then uniqueness check can assume the indices are in order
    std::ranges::sort(variantSet.Variants, std::ranges::less{}, &VariantDescriptor::Index);

    const CookError verifyUnique = VerifyVariantIndicesAreUnique(variantSet.Variants);
    if (verifyUnique != CookError::Success)
    {
        return std::unexpected(verifyUnique);
    }

    return variantSet;
}

CookError PermutationSpace::VerifyAxisNamesAreDeclared(std::span<const std::string_view> source_texts,
                                                       std::string_view module_name) const
{
    int32_t undeclaredCount = 0;

    for (const PermutationAxis& axis : axes)
    {
        bool declared = false;
        for (const std::string_view source : source_texts)
        {
            if (ContainsExternDeclarationOf(source, axis.Name))
            {
                declared = true;
                break;
            }
        }

        if (!declared)
        {
            ++undeclaredCount;
            std::println(stderr,
                         "[shader_cooker] axis '{}' has no matching `extern static const` declaration "
                         "in module {}. Slang links this symbol, nothing references it, the shader "
                         "keeps its default, and every variant cooks identical output.",
                         axis.Name,
                         module_name);
        }
    }

    if (undeclaredCount > 0)
    {
        return CookError::PermutationAxisNotDeclared;
    }

    return CookError::Success;
}

void PermutationSpace::ReportUndrivenExternConstants(std::span<const std::string_view> source_texts,
                                                     std::string_view module_name) const
{
    for (const std::string_view source : source_texts)
    {
        for (std::string_view constantName : CollectExternConstantNames(source))
        {
            if (SpaceDrivesAxisNamed(*this, constantName))
            {
                continue;
            }

            std::println(stderr,
                         "[shader_cooker] '{}' in module {} is declared extern but no axis drives it. "
                         "It keeps its declared default in every variant.",
                         constantName,
                         module_name);
        }
    }

}

CookResult<std::vector<ExternConstantDefault>> PermutationSpace::CollectUndrivenExternDefaults(
    std::span<const std::string_view> source_texts) const
{
    std::vector<ExternConstantDefault> defaults;

    for (const std::string_view source : source_texts)
    {
        for (const auto& [constName, valueText] : CollectExternConstantDefinitions(source))
        {
            if (SpaceDrivesAxisNamed(*this, constName))
            {
                continue;
            }

            const std::string_view trimmed = TrimWhitespace(valueText);
            if (trimmed == "true" || trimmed == "false")
            {
                defaults.emplace_back(std::string{ constName }, trimmed == "true" ? 1 : 0);
                continue;
            }

            const std::vector<SizeSymbol> known = AsSizeSymbols(defaults);
            const CookResult<int64_t> value = EvaluateSizeExpression(trimmed, known);
            if (!value)
            {
                std::println(stderr,
                             "[shader_cooker] could not read the default of extern constant '{}' from "
                             "'{}'. A size expression naming it would silently disagree with the shader.",
                             constName,
                             trimmed);
                return std::unexpected(value.error());
            }

            defaults.emplace_back(std::string{ constName }, value.value());
        }
    }

    return defaults;
}

const ModulePolicy* FindPolicyForModule(std::string_view module_name) noexcept
{
    for (const ModuleSpaceEntry& entry : k_ModuleSpaces)
    {
        if (entry.ModuleName == module_name)
        {
            return entry.Policy;
        }
    }

    return &k_EmptyPolicy;
}

const PermutationSpace* FindPermutationSpaceForModule(std::string_view module_name) noexcept
{
    for (const ModuleSpaceEntry& entry : k_ModuleSpaces)
    {
        if (entry.ModuleName == module_name)
        {
            return entry.Space;
        }
    }

    return &k_EmptySpace;
}

} // namespace lodestone
