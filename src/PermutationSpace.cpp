#include "PermutationSpace.hpp"
#include "SizeExpression.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <print>
#include <unordered_set>

namespace velox::cooker
{

namespace
{

    const PermutationAxis k_IfftSizeAxis{ "IFFT_SIZE",
                                          { 128u, 256u, 512u, 1024u, 2048u, 4096u, 8192u },
                                          nullptr,
                                          false };

    const PermutationAxis k_IfftUseWaveOpsAxis{ "IFFT_USE_WAVE_OPS", { false, true }, nullptr, false };

    const PermutationAxis k_IfftWaveSizeAxis{ "IFFT_WAVE_SIZE",
                                              { 16u, 32u, 64u, 128u },
                                              &k_IfftUseWaveOpsAxis,
                                              true };

    const PermutationSpace k_OceanFftSpace{ &k_IfftSizeAxis, &k_IfftUseWaveOpsAxis, &k_IfftWaveSizeAxis };

    const PermutationSpace k_EmptySpace{};

    // IfftPermuteCS reorders data and never reads a wave-op symbol, so both wave axes must stay inert
    // for it. If that ever changes, the entry point started paying for a permutation it does not use.
    const std::array<ExpectedAxisInfluence, 2> k_OceanFftExpectedInfluence{
        ExpectedAxisInfluence{ "IfftPermuteCS", "IFFT_USE_WAVE_OPS", true },
        ExpectedAxisInfluence{ "IfftPermuteCS", "IFFT_WAVE_SIZE", true }
    };

    const ModulePolicy k_OceanFftPolicy{ 64u, k_OceanFftExpectedInfluence };
    const ModulePolicy k_EmptyPolicy{};

    struct ModuleSpaceEntry
    {
        std::string_view ModuleName;
        const PermutationSpace* Space;
        const ModulePolicy* Policy;
    };

    const std::array<ModuleSpaceEntry, 1> k_ModuleSpaces{
        ModuleSpaceEntry{ "OceanFft", &k_OceanFftSpace, &k_OceanFftPolicy }
    };

    const PermutationBinding* FindBindingForAxis(const PermutationAssignment& assignment,
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

            if (line.find(k_ExternKeyword) == std::string_view::npos)
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
        constexpr std::string_view k_ExternKeyword{ "extern" };
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

            if (line.find(k_ExternKeyword) == std::string_view::npos)
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
        constexpr std::string_view k_ExternKeyword{ "extern" };
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

            if (line.find(k_ExternKeyword) == std::string_view::npos)
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
            symbols.push_back(SizeSymbol{ entry.Name, entry.Value });
        }

        return symbols;
    }

    bool SpaceDrivesAxisNamed(const PermutationSpace& space, std::string_view name) noexcept
    {
        for (const PermutationAxis* axis : space)
        {
            if (axis->Name == name)
            {
                return true;
            }
        }

        return false;
    }

    CookResult<void> VerifyVariantIndicesAreUnique(const std::vector<VariantDescriptor>& variants)
    {
        std::unordered_set<uint32_t> seen;
        seen.reserve(variants.size());

        for (const VariantDescriptor& variant : variants)
        {
            if (!seen.insert(variant.Index).second)
            {
                std::println(stderr,
                             "[shader_cooker] two variants share index {}: [{}] collides. The mixed-radix "
                             "encoding and the enumerated set disagree.",
                             variant.Index,
                             DescribeAssignment(variant.Canonical));
                return std::unexpected(CookError::PermutationVariantIndexCollision);
            }
        }

        return {};
    }

} // namespace

CookResult<std::vector<PermutationAssignment>> EnumerateActiveCombinations(const PermutationSpace& space)
{
    std::vector<PermutationAssignment> partials{ PermutationAssignment{} };

    for (const PermutationAxis* axis : space)
    {
        std::vector<PermutationAssignment> expanded;
        expanded.reserve(partials.size() * axis->Values.size());

        for (const PermutationAssignment& partial : partials)
        {
            if (axis->Parent != nullptr)
            {
                const PermutationBinding* parentBinding = FindBindingForAxis(partial, axis->Parent);
                if (parentBinding == nullptr)
                {
                    return std::unexpected(CookError::PermutationParentAxisMissing);
                }

                if (parentBinding->second != axis->RequiredParentValue)
                {
                    expanded.push_back(partial);
                    continue;
                }
            }

            for (const PermutationValue& value : axis->Values)
            {
                PermutationAssignment next = partial;
                next.emplace_back(axis, value);
                expanded.push_back(std::move(next));
            }
        }

        partials = std::move(expanded);
    }

    return partials;
}

CookResult<PermutationAssignment> CanonicalizeAssignment(const PermutationSpace& space,
                                                         const PermutationAssignment& assignment)
{
    PermutationAssignment canonical;
    canonical.reserve(space.size());

    for (const PermutationAxis* axis : space)
    {
        if (axis->Values.empty())
        {
            return std::unexpected(CookError::PermutationValueNotInAxis);
        }

        const PermutationBinding* binding = FindBindingForAxis(assignment, axis);
        const PermutationValue& value = binding != nullptr ? binding->second : axis->Values.front();
        canonical.emplace_back(axis, value);
    }

    return canonical;
}

CookResult<uint32_t> IndexOfAxisValue(const PermutationAxis& axis, const PermutationValue& value) noexcept
{
    for (size_t i = 0u; i < axis.Values.size(); ++i)
    {
        if (axis.Values[i] == value)
        {
            return static_cast<uint32_t>(i);
        }
    }

    return std::unexpected(CookError::PermutationValueNotInAxis);
}

CookResult<uint32_t> ComputeVariantIndex(const PermutationSpace& space,
                                         const PermutationAssignment& canonical)
{
    uint32_t index = 0u;

    for (const PermutationAxis* axis : space)
    {
        const PermutationBinding* binding = FindBindingForAxis(canonical, axis);
        if (binding == nullptr)
        {
            return std::unexpected(CookError::PermutationParentAxisMissing);
        }

        const CookResult<uint32_t> valueIndex = IndexOfAxisValue(*axis, binding->second);
        if (!valueIndex)
        {
            return std::unexpected(valueIndex.error());
        }

        index = index * static_cast<uint32_t>(axis->Values.size()) + valueIndex.value();
    }

    return index;
}

uint32_t ComputeVariantSpaceSize(const PermutationSpace& space) noexcept
{
    uint32_t size = 1u;

    for (const PermutationAxis* axis : space)
    {
        size *= static_cast<uint32_t>(axis->Values.size());
    }

    return size;
}

CookResult<VariantSet> EnumerateVariants(const PermutationSpace& space)
{
    const CookResult<std::vector<PermutationAssignment>> active = EnumerateActiveCombinations(space);
    if (!active)
    {
        return std::unexpected(active.error());
    }

    VariantSet variantSet;
    variantSet.Space = &space;
    variantSet.SpaceSize = ComputeVariantSpaceSize(space);
    variantSet.Variants.reserve(active.value().size());

    for (const PermutationAssignment& assignment : active.value())
    {
        const CookResult<PermutationAssignment> canonical = CanonicalizeAssignment(space, assignment);
        if (!canonical)
        {
            return std::unexpected(canonical.error());
        }

        const CookResult<uint32_t> index = ComputeVariantIndex(space, canonical.value());
        if (!index)
        {
            return std::unexpected(index.error());
        }

        VariantDescriptor descriptor;
        descriptor.Active = assignment;
        descriptor.Canonical = canonical.value();
        descriptor.Index = index.value();
        variantSet.Variants.push_back(std::move(descriptor));
    }

    const CookResult<void> uniqueResult = VerifyVariantIndicesAreUnique(variantSet.Variants);
    if (!uniqueResult)
    {
        return std::unexpected(uniqueResult.error());
    }

    std::sort(variantSet.Variants.begin(),
              variantSet.Variants.end(),
              [](const VariantDescriptor& lhs, const VariantDescriptor& rhs)
              { return lhs.Index < rhs.Index; });

    return variantSet;
}

CookResult<void> VerifyAxisNamesAreDeclared(const PermutationSpace& space,
                                            std::span<const std::string> source_texts,
                                            std::string_view module_name)
{
    uint32_t undeclaredCount = 0u;

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
                         "[shader_cooker] axis '{}' has no matching `extern const static` declaration "
                         "in module {}. Slang links this symbol, nothing references it, the shader "
                         "keeps its default, and every variant cooks identical output.",
                         axis->Name,
                         module_name);
        }
    }

    if (undeclaredCount != 0u)
    {
        return std::unexpected(CookError::PermutationAxisNotDeclared);
    }

    return {};
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
    const PermutationSpace& space,
    std::span<const std::string> source_texts)
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
    if (std::holds_alternative<bool>(value))
    {
        return std::get<bool>(value) ? 1 : 0;
    }

    if (std::holds_alternative<uint32_t>(value))
    {
        return static_cast<int64_t>(std::get<uint32_t>(value));
    }

    return static_cast<int64_t>(std::get<int32_t>(value));
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

} // namespace velox::cooker
