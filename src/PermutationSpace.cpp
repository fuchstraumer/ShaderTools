#include "PermutationSpace.hpp"
#include <algorithm>
#include <array>
#include <format>

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

    struct ModuleSpaceEntry
    {
        std::string_view ModuleName;
        const PermutationSpace* Space;
    };

    constexpr std::array<ModuleSpaceEntry, 1> k_ModuleSpaces{ ModuleSpaceEntry{ "OceanFft",
                                                                                &k_OceanFftSpace } };

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
