#pragma once
#ifndef VELOX_SHADER_COOKER_PERMUTATION_SPACE_HPP
#define VELOX_SHADER_COOKER_PERMUTATION_SPACE_HPP
#include "CookerErrors.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace velox::cooker
{

using PermutationValue = std::variant<bool, uint32_t, int32_t>;

/** One axis of variation. `Parent`/`RequiredParentValue` express a dependent axis: the axis only
 * contributes values when its parent already took the enabling value. `Name` must match the
 * `extern const static` declaration in the Slang source exactly -- a mismatch links a symbol nobody
 * references and silently leaves the shader on its default value. */
struct PermutationAxis
{
    std::string Name;
    std::vector<PermutationValue> Values;
    const PermutationAxis* Parent{ nullptr };
    PermutationValue RequiredParentValue{ false };
};

using PermutationSpace = std::vector<const PermutationAxis*>;
using PermutationBinding = std::pair<const PermutationAxis*, PermutationValue>;
using PermutationAssignment = std::vector<PermutationBinding>;

CookResult<std::vector<PermutationAssignment>> EnumerateActiveCombinations(const PermutationSpace& space);

std::string ValueToSlangLiteral(const PermutationValue& value);
std::string ValueToSlangTypeName(const PermutationValue& value);
std::string MakeExportedConstantSource(std::string_view axis_name, const PermutationValue& value);
std::string MakeVariantModuleName(std::string_view axis_name, const PermutationValue& value);
std::string MakeVariantModulePath(std::string_view axis_name, const PermutationValue& value);

std::string MakeAssignmentSuffix(const PermutationAssignment& assignment);
std::string DescribeAssignment(const PermutationAssignment& assignment);

const PermutationSpace* FindPermutationSpaceForModule(std::string_view module_name) noexcept;

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_PERMUTATION_SPACE_HPP
