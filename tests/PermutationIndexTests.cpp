#include "CookerErrors.hpp"
#include "PermutationSpace.hpp"
#include "TestHarness.hpp"

#include <cstdint>
#include <vector>

// The dense index is the key that the manifest, the generated C++, and the renderer all resolve a
// variant with. Two properties make it safe, and this file proves both without a compiler and without
// an asset.
//
// First, canonicalization can never change shader output. `Active` drives the Slang linker, and
// `Canonical` drives the index alone.
//
// Second, a caller can name only the axes it cares about. Canonicalization fills in the rest, so a
// partial assignment still resolves to one variant.
//
// The space below has the shape of the OceanFft space: two independent axes, and one axis that only
// contributes values when its parent takes the enabling value.

using lodestone::CanonicalizeAssignment;
using lodestone::ComputeVariantIndex;
using lodestone::CookError;
using lodestone::CookResult;
using lodestone::IndexOfAxisValue;
using lodestone::PermutationAssignment;
using lodestone::PermutationAxis;
using lodestone::PermutationSpace;
using lodestone::PermutationValue;
using lodestone::VariantDescriptor;
using lodestone::VariantSet;

namespace
{

const PermutationAxis k_SizeAxis{ .Name = "TEST_SIZE",
                                  .Values = { uint32_t{ 128u },
                                              uint32_t{ 256u },
                                              uint32_t{ 512u },
                                              uint32_t{ 1024u } },
                                  .Parent = nullptr,
                                  .RequiredParentValue = false };

const PermutationAxis k_UseWaveOpsAxis{ .Name = "TEST_USE_WAVE_OPS",
                                        .Values = { false, true },
                                        .Parent = nullptr,
                                        .RequiredParentValue = false };

/** Only contributes values when TEST_USE_WAVE_OPS took the value true. */
const PermutationAxis k_WaveSizeAxis{ .Name = "TEST_WAVE_SIZE",
                                      .Values = { uint32_t{ 16u }, uint32_t{ 32u }, uint32_t{ 64u } },
                                      .Parent = &k_UseWaveOpsAxis,
                                      .RequiredParentValue = true };

const PermutationSpace k_TestSpace{ &k_SizeAxis, &k_UseWaveOpsAxis, &k_WaveSizeAxis };

/** 4 sizes, times 2 wave-op settings, times 3 wave sizes. The dependent axis leaves holes in it. */
constexpr uint32_t k_ExpectedSpaceSize = 24u;
/** For each size: one variant with wave ops off, and three with wave ops on. */
constexpr uint32_t k_ExpectedVariantCount = 16u;

const lodestone::PermutationBinding* FindBinding(const PermutationAssignment& assignment,
                                                 const PermutationAxis* axis) noexcept
{
    for (const lodestone::PermutationBinding& binding : assignment)
    {
        if (binding.first == axis)
        {
            return &binding;
        }
    }

    return nullptr;
}

} // namespace

int main()
{
    lodestone::tests::TestRunner runner{ "PermutationIndexTests" };

    const CookResult<VariantSet> enumerated = lodestone::EnumerateVariants(k_TestSpace);
    if (!enumerated)
    {
        runner.Check(false, "the test space enumerates");
        return runner.Report();
    }

    const VariantSet& variants = enumerated.value();

    runner.BeginSection("the space expands to the shape the axes describe");
    runner.Check(variants.SpaceSize == k_ExpectedSpaceSize,
                 "the dense index range counts every axis, holes included");
    runner.Check(variants.Variants.size() == k_ExpectedVariantCount,
                 "a disabled dependent axis removes the variants it would have produced");

    runner.BeginSection("every index is unique and inside the range");
    std::vector<bool> seen(variants.SpaceSize, false);
    bool everyIndexIsUnique = true;
    bool everyIndexIsInRange = true;
    for (const VariantDescriptor& descriptor : variants.Variants)
    {
        if (descriptor.Index >= variants.SpaceSize)
        {
            everyIndexIsInRange = false;
            continue;
        }

        if (seen[descriptor.Index])
        {
            everyIndexIsUnique = false;
        }

        seen[descriptor.Index] = true;
    }

    runner.Check(everyIndexIsInRange, "no index reaches past the dense range");
    runner.Check(everyIndexIsUnique, "no two variants claim one index");

    runner.BeginSection("the retrieval path returns the index the variant already has");
    bool everyRoundTripAgrees = true;
    for (const VariantDescriptor& descriptor : variants.Variants)
    {
        const CookResult<PermutationAssignment> canonical =
            CanonicalizeAssignment(k_TestSpace, descriptor.Active);
        if (!canonical)
        {
            everyRoundTripAgrees = false;
            continue;
        }

        const CookResult<uint32_t> index = ComputeVariantIndex(k_TestSpace, canonical.value());
        if (!index || index.value() != descriptor.Index)
        {
            everyRoundTripAgrees = false;
        }
    }

    runner.Check(everyRoundTripAgrees,
                 "canonicalizing Active and indexing it returns the descriptor's own index");

    runner.BeginSection("a disabled axis leaves Active and fills Canonical");
    const VariantDescriptor* waveOpsOff = nullptr;
    for (const VariantDescriptor& descriptor : variants.Variants)
    {
        const lodestone::PermutationBinding* useWaveOps = FindBinding(descriptor.Active, &k_UseWaveOpsAxis);
        if (useWaveOps != nullptr && useWaveOps->second == PermutationValue{ false })
        {
            waveOpsOff = &descriptor;
            break;
        }
    }

    runner.Check(waveOpsOff != nullptr, "the space produces a variant with wave ops off");
    if (waveOpsOff != nullptr)
    {
        runner.Check(FindBinding(waveOpsOff->Active, &k_WaveSizeAxis) == nullptr,
                     "a disabled axis is absent from Active, so it cannot reach the linker");
        runner.Check(waveOpsOff->Active.size() == 2u, "Active holds only the axes the variant uses");
        runner.Check(waveOpsOff->Canonical.size() == k_TestSpace.size(),
                     "Canonical holds every axis in the space");

        const lodestone::PermutationBinding* canonicalWaveSize =
            FindBinding(waveOpsOff->Canonical, &k_WaveSizeAxis);
        runner.Check(canonicalWaveSize != nullptr &&
                         canonicalWaveSize->second == k_WaveSizeAxis.Values.front(),
                     "a disabled axis takes its first value in Canonical");
    }

    runner.BeginSection("a partial assignment resolves to one variant");
    // The design claim: name only the axes you care about, and canonicalization supplies the rest.
    PermutationAssignment partial;
    partial.emplace_back(&k_SizeAxis, PermutationValue{ uint32_t{ 512u } });

    const CookResult<PermutationAssignment> filled = CanonicalizeAssignment(k_TestSpace, partial);
    runner.Check(filled.has_value(), "a partial assignment canonicalizes");
    if (filled)
    {
        const CookResult<uint32_t> partialIndex = ComputeVariantIndex(k_TestSpace, filled.value());
        runner.Check(partialIndex.has_value(), "a canonicalized partial assignment has an index");

        bool matchesRealVariant = false;
        for (const VariantDescriptor& descriptor : variants.Variants)
        {
            if (partialIndex && descriptor.Index == partialIndex.value())
            {
                matchesRealVariant = true;
            }
        }

        runner.Check(matchesRealVariant, "the partial assignment names a variant the cook produced");
    }

    runner.BeginSection("a value outside an axis is an error, not a default");
    const CookResult<uint32_t> missingValue =
        IndexOfAxisValue(k_SizeAxis, PermutationValue{ uint32_t{ 777u } });
    runner.Check(!missingValue && missingValue.error() == CookError::PermutationValueNotInAxis,
                 "a value the axis does not hold is rejected by name");

    const CookResult<uint32_t> wrongType =
        IndexOfAxisValue(k_SizeAxis, PermutationValue{ int32_t{ 128 } });
    runner.Check(!wrongType && wrongType.error() == CookError::PermutationValueNotInAxis,
                 "an int32 128 does not match a uint32 128, because the variant compares its type too");

    return runner.Report();
}
