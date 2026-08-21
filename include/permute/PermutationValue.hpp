#pragma once
#ifndef LODESTONE_PERMUTATION_VALUE_HPP
#define LODESTONE_PERMUTATION_VALUE_HPP
#include <cstdint>

namespace lodestone
{

// We used to use std::variant, but we know that our permutation values have a fixed set of types, so we can
// represent them more efficiently than a variant. mostly, less templates and stdlib includes
struct PermutationValue
{
    enum class Type : uint8_t
    {
        Invalid = 0,
        Bool,
        UInt,
        SInt
    };

    constexpr PermutationValue() noexcept : type(Type::Invalid), uintValue(static_cast<uint32_t>(0)) {}
    constexpr explicit PermutationValue(bool value) noexcept : type(Type::Bool), boolValue(value) {}
    constexpr explicit PermutationValue(uint32_t value) noexcept : type(Type::UInt), uintValue(value) {}
    constexpr explicit PermutationValue(int32_t value) noexcept : type(Type::SInt), sintValue(value) {}


    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] Type GetType() const noexcept;
    [[nodiscard]] bool AsBool() const noexcept;
    [[nodiscard]] uint32_t AsUInt() const noexcept;
    [[nodiscard]] int32_t AsSInt() const noexcept;

    [[nodiscard]] bool operator==(const PermutationValue& other) const noexcept;
    [[nodiscard]] bool operator!=(const PermutationValue& other) const noexcept;
    [[nodiscard]] bool operator<(const PermutationValue& other) const noexcept;

private:
    Type type;
    //NOLINTBEGIN(readability-identifier-naming)
    union
    {
        uint32_t uintValue;
        int32_t sintValue;
        bool boolValue;
    };
    //NOLINTEND(readability-identifier-naming)
};

}

#endif // !LODESTONE_PERMUTATION_VALUE_HPP
