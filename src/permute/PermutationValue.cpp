#include "permute/PermutationValue.hpp"
#include <cstdint>

namespace lodestone
{
    bool PermutationValue::IsValid() const noexcept
    {
        return type != Type::Invalid;
    }

    PermutationValue::Type PermutationValue::GetType() const noexcept
    {
        return type;
    }

    //NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
    bool PermutationValue::AsBool() const noexcept
    {
        return boolValue;
    }

    uint32_t PermutationValue::AsUInt() const noexcept
    {
        return uintValue;
    }

    int32_t PermutationValue::AsSInt() const noexcept
    {
        return sintValue;
    }

    bool PermutationValue::operator==(const PermutationValue& other) const noexcept
    {
        if (type != other.type)
        {
            return false;
        }

        switch (type)
        {
        case Type::Bool:
            return boolValue == other.boolValue;
        case Type::UInt:
            return uintValue == other.uintValue;
        case Type::SInt:
            return sintValue == other.sintValue;
        case Type::Invalid:
            return true;
        }

        return false;
    }

    bool PermutationValue::operator!=(const PermutationValue& other) const noexcept
    {
        return !(*this == other);
    }

    bool PermutationValue::operator<(const PermutationValue& other) const noexcept
    {
        if (type != other.type)
        {
            return type < other.type;
        }

        switch (type)
        {
        case Type::Bool:
            return static_cast<int>(boolValue) < static_cast<int>(other.boolValue);
        case Type::UInt:
            return uintValue < other.uintValue;
        case Type::SInt:
            return sintValue < other.sintValue;
        case Type::Invalid:
            return false;
        }

        return false;
    }

    //NOLINTEND(cppcoreguidelines-pro-type-union-access)
}
