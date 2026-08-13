#pragma once
#ifndef VELOX_SHADER_COOKER_ERRORS_HPP
#define VELOX_SHADER_COOKER_ERRORS_HPP
#include <cstdint>
#include <expected>
#include <string_view>

namespace velox::cooker
{

enum class CookError : uint16_t
{
    Invalid = 0,
    Success = 1,
    GlobalSessionCreationFailed = 10,
    SessionCreationFailed = 11,
    ModuleLoadFailed = 12,
    EntryPointEnumerationFailed = 13,
    VariantModuleCreationFailed = 14,
    CompositeCreationFailed = 15,
    LinkFailed = 16,
    CodeGenerationFailed = 17,
    CompilerNotInitialized = 18,

    ReflectionUnavailable = 40,
    ReflectionMismatch = 41,
    ReflectionSizeUnresolved = 42,
    SizeExpressionParseFailed = 43,
    SizeExpressionUnknownSymbol = 44,
    SizeExpressionDivideByZero = 45,
    SizeExpressionOutOfRange = 46,

    NoModulesSpecified = 60,
    NoOutputSpecified = 61,
    UnknownArgument = 62,
    MalformedArgument = 63,

    PermutationSpaceNotFound = 80,
    PermutationParentAxisMissing = 81,
    PermutationValueNotInAxis = 82,
    PermutationAxisNotDeclared = 83,
    PermutationVariantIndexCollision = 84,

    LibraryRoundTripFailed = 90,
    CookNotDeterministic = 91,
    ModulePolicyViolated = 92,

    OutputPathInvalid = 100,
    OutputWriteFailed = 101,
};

template<typename T>
using CookResult = std::expected<T, CookError>;

std::string_view ToString(CookError error) noexcept;

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_ERRORS_HPP
