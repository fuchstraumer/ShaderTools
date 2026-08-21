#pragma once
#ifndef LODESTONE_PERMUTATION_AXIS_HPP
#define LODESTONE_PERMUTATION_AXIS_HPP
#include "PermutationValue.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>

namespace lodestone
{

struct PermutationAxis
{
    /**This helps control permutation explosions, mostly. todo-ship: make it a cmake configure opt */
    static constexpr std::size_t k_MaxValues = 8u;
    /** `ParentIndex` of an axis with no parent. Such an axis is always active. */
    static constexpr int32_t k_NoParent = -1;

    PermutationAxis(std::string _name,
                    std::span<const PermutationValue> _values,
                    int32_t _parentIndex,
                    PermutationValue _requiredParentValue) noexcept;
    // initializer_list will be removed once we get to the data-driven permutation system
    // this just keeps things compiling and running, for now
    PermutationAxis(std::string _name,
                    std::initializer_list<PermutationValue> _values,
                    int32_t _parentIndex,
                    PermutationValue _requiredParentValue) noexcept;

    std::string Name;
    /** Where the parent axis sits in the space that owns this axis. An index and not a pointer: the
     * space holds its axes by value, so a copy of the space would leave a pointer that still aims at
     * the axis of the original. */
    int32_t ParentIndex{ k_NoParent };
    PermutationValue RequiredParentValue;

    [[nodiscard]] bool HasParent() const noexcept;
    [[nodiscard]] int64_t NumValues() const noexcept;
    [[nodiscard]] std::span<const PermutationValue> GetValues() const noexcept;
    [[nodiscard]] const PermutationValue& GetDefault() const noexcept;

private:
    int64_t numValues{ -1 };
    std::array<PermutationValue, k_MaxValues> values;
};

}

#endif // LODESTONE_PERMUTATION_AXIS_HPP