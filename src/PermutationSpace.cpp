#include "PermutationSpace.hpp"
#include "CookerErrors.hpp"
#include "SizeExpression.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <format>
#include <functional>
#include <iterator>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace lodestone
{

namespace
{

    const PermutationAxis k_IfftSizeAxis{ .Name = "IFFT_SIZE",
                                          .Values = { 128u, 256u, 512u, 1024u, 2048u, 4096u, 8192u },
                                          .Parent = nullptr,
                                          .RequiredParentValue = false };

    const PermutationAxis k_IfftUseWaveOpsAxis{ .Name = "IFFT_USE_WAVE_OPS",
                                                .Values = { false, true },
                                                .Parent = nullptr,
                                                .RequiredParentValue = false };

    const PermutationAxis k_IfftWaveSizeAxis{ .Name = "IFFT_WAVE_SIZE",
                                              .Values = { 16u, 32u, 64u, 128u },
                                              .Parent = &k_IfftUseWaveOpsAxis,
                                              .RequiredParentValue = true };

    const PermutationSpace k_OceanFftSpace{ &k_IfftSizeAxis, &k_IfftUseWaveOpsAxis, &k_IfftWaveSizeAxis };

    const PermutationSpace k_EmptySpace{};

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
                                                    return binding.first == axis;
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

            size_t valueStart = assignIndex + 1u;
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
        return std::ranges::any_of(space,
                                   [name](const PermutationAxis* axis)
                                   {
                                       return axis->Name == name;
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

// perform CCSP with classic backtracking, but skip any axis whose parent is not active
// this is a somewhat embarassing amount of commenting for me, but I have not done constraint satisfaction
// formally *ever* before, and I want to make sure I understand it. these are notes for me. i am not a learned
// woman
CookResult<std::vector<PermutationAssignment>> EnumerateActiveCombinations(const PermutationSpace& space)
{
    std::vector<PermutationAssignment> partials{ PermutationAssignment{} };
    partials.reserve(space.front()->Values.size());
    for (const PermutationAxis* axis : space)
    {
        // Despite having to do recursive work here, we can at least reserve the right amount of space. I
        // guess.
        std::vector<PermutationAssignment> expanded;
        expanded.reserve(partials.size() * axis->Values.size());
        // For the current axis, we need to traverse every partial assignment (incomplete combination) we have
        // thus far and expand/evaluate it for the current axis. This is a breadth-first search of the
        // combination space, and we will continue to expand the partials until we have a complete assignment
        // for every axis in the space.
        for (const PermutationAssignment& partial : partials)
        {
            // If an axis has a parent, we must check that parent to know whether to expand this current axis
            // If there is no parent, we proceed to just expand the axis as normal
            if (axis->Parent != nullptr)
            {
                // Read the current list of active axes to see if the parent is active, and retrieve
                // it if it is. If the parent is not active (present), there is an axis declaration
                // order error.
                const PermutationBinding* parentBinding = FindBindingForAxis(partial, axis->Parent);
                if (parentBinding == nullptr)
                {
                    // todo-ship: This is a user error, and should be evaluated during initial load when we're
                    // already traversing permutations to check for undriven values, etc. Flatten this
                    // calltree to use less Results
                    return std::unexpected(CookError::PermutationParentAxisMissing);
                }
                // If the parent is active, but not set to the required value, close partial off
                // for this current partial (where parent axis was evaluated to the wrong value)
                if (parentBinding->second != axis->RequiredParentValue)
                {
                    expanded.push_back(partial);
                    continue;
                }
            }

            // Expand the current axis, evaluating/instantiating it for each of it's values
            // We store the axis (the abstract half) and the *value* (the concrete half). This
            // defines a *Binding* or unique instantiation of the axis for this current partial.
            // (thus a binding is just {abstract [axis*], concrete [value]})
            for (const PermutationValue& value : axis->Values)
            {
                // at each depth, we take the current partial as our starting point (as that's how
                // breadth-first constraint satisfaction like this works best for our data)
                PermutationAssignment next = partial;
                next.emplace_back(axis, value);
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
PermutationAssignment CanonicalizeAssignment(const PermutationSpace& space,
                                             const PermutationAssignment& assignment)
{
    PermutationAssignment canonical;
    canonical.reserve(space.size());
    // So, as mentioned above: step through each axis.
    for (const PermutationAxis* axis : space)
    {
        // Get the binding (concrete instantiation) of the current axis for *this* assignment.
        const PermutationBinding* binding = FindBindingForAxis(assignment, axis);
        // Now check: did we fail to find the binding? That means it was folded out of the assignment,
        // because one of it's dependent axes values was not set as needed. Thus, default value assigned.
        const PermutationValue value = binding != nullptr ? binding->second : axis->Values.front();
        canonical.emplace_back(axis, value);
    }
    // And bam, the canonical assignment is just a fully "concrete" instance of the *actual* active assignment
    return canonical;
}

int32_t ComputeVariantIndex(const PermutationSpace& space, const PermutationAssignment& canonical)
{
    std::ptrdiff_t index = 0;

    for (size_t i = 0; i < space.size(); ++i)
    {
        // in canonical, the i-th element corresponds to the i-th axis in the space.
        const PermutationBinding& binding = canonical[i];
        const PermutationAxis* axis = binding.first;
        const PermutationValue& value = binding.second;
        const auto found = std::ranges::find(axis->Values, value);
        const std::ptrdiff_t valueIndex = std::distance(axis->Values.begin(), found);
        index = (index * std::ssize(axis->Values)) + valueIndex;
    }

    return static_cast<int32_t>(index);
}

int32_t ComputeVariantSpaceSize(const PermutationSpace& space) noexcept
{
    int32_t size = 1;

    for (const PermutationAxis* axis : space)
    {
        size *= static_cast<int32_t>(std::ssize(axis->Values));
    }

    return size;
}

CookResult<VariantSet> EnumerateVariants(const PermutationSpace& space)
{
    CookResult<std::vector<PermutationAssignment>> enumerateActiveResult = EnumerateActiveCombinations(space);
    if (!enumerateActiveResult)
    {
        return std::unexpected(enumerateActiveResult.error());
    }

    std::vector<PermutationAssignment> active{ std::move(enumerateActiveResult.value()) };

    VariantSet variantSet;
    variantSet.Space = &space;
    variantSet.SpaceSize = ComputeVariantSpaceSize(space);
    variantSet.Variants.reserve(active.size());

    for (PermutationAssignment& assignment : active)
    {
        PermutationAssignment canonical = CanonicalizeAssignment(space, assignment);
        const int32_t index = ComputeVariantIndex(space, canonical);
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

CookError VerifyAxisNamesAreDeclared(const PermutationSpace& space,
                                     std::span<const std::string> source_texts,
                                     std::string_view module_name)
{
    int32_t undeclaredCount = -1;

    for (const PermutationAxis* axis : space)
    {
        bool declared = false;
        for (const std::string& source : source_texts)
        {
            if (ContainsExternDeclarationOf(source, axis->Name))
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
                         axis->Name,
                         module_name);
        }
    }

    if (undeclaredCount > 0)
    {
        return CookError::PermutationAxisNotDeclared;
    }

    return CookError::Success;
}

void ReportUndrivenExternConstants(const PermutationSpace& space,
                                   std::span<const std::string> source_texts,
                                   std::string_view module_name)
{
    for (const std::string& source : source_texts)
    {
        for (std::string_view name : CollectExternConstantNames(source))
        {
            if (SpaceDrivesAxisNamed(space, name))
            {
                continue;
            }

            std::println(stderr,
                         "[shader_cooker] '{}' in module {} is declared extern but no axis drives it. "
                         "It keeps its declared default in every variant.",
                         name,
                         module_name);
        }
    }
}

CookResult<std::vector<ExternConstantDefault>> CollectUndrivenExternDefaults(
    const PermutationSpace& space, std::span<const std::string> source_texts)
{
    std::vector<ExternConstantDefault> defaults;

    for (const std::string& source : source_texts)
    {
        for (const auto& [name, valueText] : CollectExternConstantDefinitions(source))
        {
            if (SpaceDrivesAxisNamed(space, name))
            {
                continue;
            }

            const std::string_view trimmed = TrimWhitespace(valueText);
            if (trimmed == "true" || trimmed == "false")
            {
                defaults.emplace_back(std::string{ name }, trimmed == "true" ? 1 : 0);
                continue;
            }

            const std::vector<SizeSymbol> known = AsSizeSymbols(defaults);
            const CookResult<int64_t> value = EvaluateSizeExpression(trimmed, known);
            if (!value)
            {
                std::println(stderr,
                             "[shader_cooker] could not read the default of extern constant '{}' from "
                             "'{}'. A size expression naming it would silently disagree with the shader.",
                             name,
                             trimmed);
                return std::unexpected(value.error());
            }

            defaults.emplace_back(std::string{ name }, value.value());
        }
    }

    return defaults;
}

int64_t PermutationValueToInt64(const PermutationValue& value) noexcept
{
    if (const bool* booleanValue = std::get_if<bool>(&value); booleanValue != nullptr)
    {
        return *booleanValue ? 1 : 0;
    }

    if (const uint32_t* unsignedValue = std::get_if<uint32_t>(&value); unsignedValue != nullptr)
    {
        return static_cast<int64_t>(*unsignedValue);
    }

    if (const int32_t* signedValue = std::get_if<int32_t>(&value); signedValue != nullptr)
    {
        return static_cast<int64_t>(*signedValue);
    }

    return 0;
}

std::string ValueToSlangLiteral(const PermutationValue& value)
{
    if (std::holds_alternative<bool>(value))
    {
        return std::get<bool>(value) ? "true" : "false";
    }

    if (std::holds_alternative<uint32_t>(value))
    {
        return std::to_string(std::get<uint32_t>(value));
    }

    return std::to_string(std::get<int32_t>(value));
}

std::string ValueToSlangTypeName(const PermutationValue& value)
{
    if (std::holds_alternative<bool>(value))
    {
        return "bool";
    }

    if (std::holds_alternative<uint32_t>(value))
    {
        return "uint";
    }

    return "int";
}

std::string MakeExportedConstantSource(std::string_view axis_name, const PermutationValue& value)
{
    return std::format("export static const {} {} = {};\n",
                       ValueToSlangTypeName(value),
                       axis_name,
                       ValueToSlangLiteral(value));
}

std::string MakeVariantModuleName(std::string_view axis_name, const PermutationValue& value)
{
    return std::format("{}_{}", axis_name, ValueToSlangLiteral(value));
}

std::string MakeVariantModulePath(std::string_view axis_name, const PermutationValue& value)
{
    return std::format("{}_{}.slang", axis_name, ValueToSlangLiteral(value));
}

std::string MakeAssignmentSuffix(const PermutationAssignment& assignment)
{
    std::string suffix;
    suffix.reserve(8u * assignment.size());

    for (const PermutationBinding& binding : assignment)
    {
        suffix += std::format("_{}", ValueToSlangLiteral(binding.second));
    }

    return suffix;
}

std::string DescribeAssignment(const PermutationAssignment& assignment)
{
    std::string description;
    description.reserve(24u * assignment.size());

    for (const PermutationBinding& binding : assignment)
    {
        if (!description.empty())
        {
            description += ", ";
        }
        description += std::format("{}={}", binding.first->Name, ValueToSlangLiteral(binding.second));
    }

    return description;
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
