#include "permute/PermutationAxis.hpp"
#include "permute/PermutationValue.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <utility>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-container"
#endif

namespace lodestone
{

PermutationAxis::PermutationAxis(std::string _name,
                                 std::span<const PermutationValue> _values,
                                 int32_t _parentIndex,
                                 PermutationValue _requiredParentValue) noexcept
    : Name(std::move(_name)),
      ParentIndex(_parentIndex),
      RequiredParentValue(_requiredParentValue)
{
    numValues = static_cast<int64_t>(std::min(_values.size(), values.size()));
    std::copy_n(_values.begin(), numValues, values.begin());
}

PermutationAxis::PermutationAxis(std::string _name,
                                 std::initializer_list<PermutationValue> _values,
                                 int32_t _parentIndex,
                                 PermutationValue _requiredParentValue) noexcept
    : PermutationAxis(std::move(_name),
                      std::span<const PermutationValue>{ _values.begin(), _values.size() },
                      _parentIndex,
                      _requiredParentValue)
{
}

int64_t PermutationAxis::NumValues() const noexcept
{
    return numValues;
}

std::span<const PermutationValue> PermutationAxis::GetValues() const noexcept
{
    return std::span<const PermutationValue>{ values.data(), static_cast<size_t>(numValues) };
}

const PermutationValue& PermutationAxis::GetDefault() const noexcept
{
    return values.front();
}

bool PermutationAxis::HasParent() const noexcept
{
    return ParentIndex != k_NoParent;
}

} // namespace lodestone
