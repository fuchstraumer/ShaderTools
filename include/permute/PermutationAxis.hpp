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

    PermutationAxis(std::string _name,
                    std::span<const PermutationValue> _values,
                    const PermutationAxis* _parent,
                    PermutationValue _requiredParentValue) noexcept;
    // initializer_list will be removed once we get to the data-driven permutation system
    // this just keeps things compiling and running, for now
    PermutationAxis(std::string _name,
                    std::initializer_list<PermutationValue> _values,
                    const PermutationAxis* _parent,
                    PermutationValue _requiredParentValue) noexcept;

    std::string Name;
    const PermutationAxis* Parent;
    PermutationValue RequiredParentValue;

    [[nodiscard]] int64_t NumValues() const noexcept;
    [[nodiscard]] std::span<const PermutationValue> GetValues() const noexcept;
    [[nodiscard]] const PermutationValue& GetDefault() const noexcept;

private:
    int64_t numValues{ -1 };
    std::array<PermutationValue, k_MaxValues> values;
};

}

#endif // LODESTONE_PERMUTATION_AXIS_HPP