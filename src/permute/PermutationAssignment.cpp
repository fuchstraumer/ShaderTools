#include "permute/PermutationAssignment.hpp"
#include "permute/PermutationValue.hpp"
#include <cstddef>
#include <format>
#include <string>
#include <utility>

namespace lodestone
{

CanonicalAssignment::operator const PermutationAssignment&() const noexcept
{
    return values;
}

std::size_t CanonicalAssignment::size() const noexcept
{
    return values.size();
}

const PermutationBinding& CanonicalAssignment::operator[](std::size_t index) const noexcept
{
    return values[index];
}

CanonicalAssignment::CanonicalAssignment(PermutationAssignment&& canonical) noexcept
    : values{ std::move(canonical) }
{
}

std::string MakeAssignmentSuffix(const PermutationAssignment& assignment)
{
    std::string suffix;
    suffix.reserve(8u * assignment.size());

    for (const PermutationBinding& binding : assignment)
    {
        suffix += std::format("_{}", ValueToSlangLiteral(binding.Value));
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
        description += std::format("{}={}", binding.Axis->Name, ValueToSlangLiteral(binding.Value));
    }

    return description;
}

}
