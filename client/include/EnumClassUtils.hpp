#pragma once
#ifndef VELOX_UTILITY_ENUM_CLASS_UTILS_HPP
#define VELOX_UTILITY_ENUM_CLASS_UTILS_HPP
#include <type_traits>
#include <utility>

/**
 * @file EnumClassUtils.hpp
 * @brief Utilities for working with enum class bitflags in a type-safe manner.
 * Defines a macro to generate bitwise operators for enum classes used as bitmasks, and a shim that allows
 * for safe boolean checks on the results of these operations (while preserving stored value in bitmask).
*/

template<typename T>
    requires(std::is_enum_v<T> and requires(T e) { enable_bitmask_operator_or(e); })
constexpr auto operator|(const T lhs, const T rhs) noexcept
{
    return static_cast<T>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

template<typename T>
    requires(std::is_enum_v<T> and requires(T e) { enable_bitmask_operator_and(e); })
constexpr auto operator&(const T lhs, const T rhs) noexcept
{
    return static_cast<T>(std::to_underlying(lhs) & std::to_underlying(rhs));
}

template<typename T>
    requires(std::is_enum_v<T> and requires(T e) { enable_bitmask_operator_xor(e); })
constexpr auto operator^(const T lhs, const T rhs) noexcept
{
    return static_cast<T>(std::to_underlying(lhs) ^ std::to_underlying(rhs));
}

// can we use this to define common patterns like |= ? 
template<typename T>
    requires(std::is_enum_v<T> and requires(T e) { enable_bitmask_operator_or_eq(e); })
constexpr T& operator|=(T& lhs, const T rhs) noexcept
{
    lhs = static_cast<T>(std::to_underlying(lhs) | std::to_underlying(rhs));
    return lhs;
}

// add ~ mask option
template<typename T>
    requires(std::is_enum_v<T> and requires(T e) { enable_bitmask_operator_not(e); })
constexpr auto operator~(const T value) noexcept
{
    return static_cast<T>(~std::to_underlying(value));
}

// pairs with ~ for the clear-a-flag idiom: usage &= ~Flags::MapWrite
template<typename T>
    requires(std::is_enum_v<T> and requires(T e) { enable_bitmask_operator_and(e); })
constexpr T& operator&=(T& lhs, const T rhs) noexcept
{
    lhs = static_cast<T>(std::to_underlying(lhs) & std::to_underlying(rhs));
    return lhs;
}

template<typename T>
    requires(std::is_enum_v<T> and requires(T e) { enable_bitmask_operator_xor(e); })
constexpr T& operator^=(T& lhs, const T rhs) noexcept
{
    lhs = static_cast<T>(std::to_underlying(lhs) ^ std::to_underlying(rhs));
    return lhs;
}

/** @brief True when every bit in flags is set in value. The right test for a single-bit flag, and the
 * one that reads correctly when passed a multi-bit alias like AllTransfer.
 * @note Use this rather than `value & flag` in a condition - the operators above return the enum type,
 * which has no implicit conversion to bool.
 */
template<typename T>
    requires std::is_enum_v<T>
constexpr bool HasAllFlags(T value, T flags) noexcept
{
    return (std::to_underlying(value) & std::to_underlying(flags)) == std::to_underlying(flags);
}

/** @brief True when any bit in flags is set in value. Differs from HasAllFlags only for multi-bit
 * arguments, which is exactly where guessing wrong is silent - hence two names rather than one.
 */
template<typename T>
    requires std::is_enum_v<T>
constexpr bool HasAnyFlag(T value, T flags) noexcept
{
    return (std::to_underlying(value) & std::to_underlying(flags)) != 0;
}

// now our macro is a bunch of consteval functions that return true for the operators we want to enable
#define MAKE_ENUM_CLASS_FLAGS(EnumClass) \
    consteval bool enable_bitmask_operator_or(EnumClass) { return true; } \
    consteval bool enable_bitmask_operator_and(EnumClass) { return true; } \
    consteval bool enable_bitmask_operator_xor(EnumClass) { return true; } \
    consteval bool enable_bitmask_operator_or_eq(EnumClass) { return true; } \
    consteval bool enable_bitmask_operator_not(EnumClass) { return true; }

#endif // !VELOX_UTILITY_ENUM_CLASS_UTILS_HPP
