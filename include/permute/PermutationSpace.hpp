#pragma once
#ifndef LODESTONE_PERMUTATION_SPACE_HPP
#define LODESTONE_PERMUTATION_SPACE_HPP
#include "CookerErrors.hpp"
#include "permute/PermutationValue.hpp"
#include "permute/PermutationAxis.hpp"
#include "permute/PermutationPolicy.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


namespace lodestone
{

using PermutationSpace = std::vector<const PermutationAxis*>;
using PermutationBinding = std::pair<const PermutationAxis*, PermutationValue>;
using PermutationAssignment = std::vector<PermutationBinding>;

/** An assignment that holds every axis of one space, in declaration order. Only
 * `CanonicalizeAssignment` builds one, so a partial assignment cannot reach `ComputeVariantIndex` and
 * return a plausible wrong index. The conversion to `PermutationAssignment` runs one way only. */
class CanonicalAssignment
{
public:
    CanonicalAssignment() noexcept = default;

    [[nodiscard]] operator const PermutationAssignment&() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept; //NOLINT(readability-identifier-naming)
    [[nodiscard]] const PermutationBinding& operator[](std::size_t index) const noexcept;

private:
    friend CanonicalAssignment CanonicalizeAssignment(const PermutationSpace& space,
                                                      const PermutationAssignment& assignment);
    explicit CanonicalAssignment(PermutationAssignment&& canonical) noexcept;

    PermutationAssignment values;
};

/**
 * One variant's identity.
 *
 * `Active`: Contains only the axes this variant actually uses. The linker uses this to export the final
 * values (i.e., what shows up in source)
 *
 * `Canonical`: A complete list of EVERY axis in the permutation space, in declaration order. This is
 * *not* what the shader will actually use.
 *
 * If an axis is disabled, it will not appear in Active but it will appear in Canonical. This happens with
 * an axis (or axes) whose parent does not match the value required to enable it. The permutation evaluator
 * will fill in the first value of that axis as the value in Canonical.
 *
 * Why? Because this then provides a truly stable index for the variant, which several properties/values we
 * associate with a variant are computed. This also makes it simpler for further retrieval: we don't need to
 * know the fully evaluated "correct" value of each axis to retrieve it, we can just use our unique values
 * and the canonicalized values to retrieve the variant. Think how trivial that is: if you know just the
 * set of values you want to use, you can get your variant.
 */
struct VariantDescriptor
{
    PermutationAssignment Active;
    CanonicalAssignment Canonical;
    int32_t Index{ 0 };
};

/** Everything one permutation space expands to. `SpaceSize` counts the dense index range, holes
 * included: a disabled dependent axis leaves gaps, and the plan accepts them rather than pay for a
 * lookup structure. */
struct VariantSet
{
    const PermutationSpace* Space{ nullptr };
    std::vector<VariantDescriptor> Variants;
    int32_t SpaceSize{ 0u };
};

CookResult<std::vector<PermutationAssignment>> EnumerateActiveCombinations(const PermutationSpace& space);
CookResult<VariantSet> EnumerateVariants(const PermutationSpace& space);

CanonicalAssignment CanonicalizeAssignment(const PermutationSpace& space, const PermutationAssignment& assignment);
int32_t ComputeVariantIndex(const PermutationSpace& space, const CanonicalAssignment& canonical);
int32_t ComputeVariantSpaceSize(const PermutationSpace& space) noexcept;

/**Every axis name must match an `extern static const` declaration in the shader. A mismatch links a
 * symbol nobody references, leaves the shader on its default, and errors nowhere -- this will result in
 * a set of variants with duplicate source code and behavior, when we explicitly don't want that. */
[[nodiscard]] CookError VerifyAxisNamesAreDeclared(const PermutationSpace& space,
                                                   std::span<const std::string> source_texts,
                                                   std::string_view module_name);

/**The other direction: an `extern` constant that no axis drives keeps its default in every variant.
 * This is what our resource sizing annotations rely on, in the Slang compiler machinery (though they
 * don't actually affect source code: they just carry through to the data we extract still) */
void ReportUndrivenExternConstants(const PermutationSpace& space,
                                   std::span<const std::string> source_texts,
                                   std::string_view module_name);

struct ExternConstantDefault
{
    std::string Name;
    int64_t Value{ 0 };
};

/**The declared default of every `extern` constant no axis drives. A size expression may name these,
 * and the value it gets is the one the shader really compiled with, because nothing overrode it.
 * Axis-driven constants are excluded: their value is per-variant and comes from the assignment. */
CookResult<std::vector<ExternConstantDefault>> CollectUndrivenExternDefaults(
    const PermutationSpace& space, std::span<const std::string> source_texts);

/** Widens any axis value to the integer type the size-expression evaluator works in. A `bool` axis
 * becomes 0 or 1, which is what a shader comparing it against a constant would see. */
int64_t PermutationValueToInt64(const PermutationValue& value) noexcept;

std::string ValueToSlangLiteral(const PermutationValue& value);
std::string ValueToSlangTypeName(const PermutationValue& value);
std::string MakeExportedConstantSource(std::string_view axis_name, const PermutationValue& value);
std::string MakeVariantModuleName(std::string_view axis_name, const PermutationValue& value);
std::string MakeVariantModulePath(std::string_view axis_name, const PermutationValue& value);

std::string MakeAssignmentSuffix(const PermutationAssignment& assignment);
std::string DescribeAssignment(const PermutationAssignment& assignment);

const ModulePolicy* FindPolicyForModule(std::string_view module_name) noexcept;
const PermutationSpace* FindPermutationSpaceForModule(std::string_view module_name) noexcept;

} // namespace lodestone

#endif // !LODESTONE_PERMUTATION_SPACE_HPP
