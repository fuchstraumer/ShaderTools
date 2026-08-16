#include "TestHarness.hpp"

#include "CookedLibrary.hpp"
#include "DedupeReport.hpp"
#include "PermutationSpace.hpp"
#include "ShaderDataSchema.hpp"
#include "ShaderLibraryTypes.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

/** Proves that dedup changes what the tables cost and never what the cook measures.
 *
 * `--no-dedupe` is the A/B control arm, so both arms must reach the same conclusions about the
 * shader. Every measurement therefore has to read content. A measurement that reads an interner index
 * gives one answer with dedup on and the opposite answer with dedup off, and the module then fails
 * its own policy in the arm that exists to debug the other one.
 *
 * This test needs no Slang, no compiler, and no asset. */
using namespace lodestone;

namespace
{

constexpr std::string_view k_ActiveEntryPoint = "ActiveCS";
constexpr std::string_view k_InertEntryPoint = "InertCS";

PermutationAxis MakeBoolAxis(std::string name)
{
    PermutationAxis axis;
    axis.Name = std::move(name);
    axis.Values = { PermutationValue{ false }, PermutationValue{ true } };
    return axis;
}

ReflectedBinding MakeSharedBinding()
{
    ReflectedBinding binding;
    binding.Name = "Waves";
    binding.Group = 0u;
    binding.Binding = 0u;
    binding.Kind = BindingKind::StorageBuffer;
    binding.EntryPointUsageMask = 0u;
    binding.ElementStride = 16u;
    binding.ArrayCount = 1u;
    binding.Shape = ResourceShape::Buffer;
    return binding;
}

CompiledEntryPoint MakeEntryPoint(std::string_view name, std::string code)
{
    CompiledEntryPoint entryPoint;
    entryPoint.Name = name;
    entryPoint.Code = std::move(code);
    entryPoint.Reflection.Name = entryPoint.Name;
    entryPoint.Reflection.Stage = ShaderStageKind::Compute;
    entryPoint.Reflection.Workgroup = WorkgroupSize{ .X = 64u, .Y = 1u, .Z = 1u };
    entryPoint.Reflection.UsedBindingIndices.push_back(0u);
    return entryPoint;
}

/** `ActiveCS` reads the first axis and nothing else. `InertCS` reads neither. So the correct answer
 * is fixed by construction, and it does not depend on how the tables were filled. */
CompiledVariant MakeVariant(uint32_t index, bool first_axis_value, bool second_axis_value)
{
    CompiledVariant variant;
    variant.VariantIndex = index;
    variant.VariantSuffix = std::format("_{}_{}", first_axis_value, second_axis_value);
    variant.VariantDescription = std::format("AXIS_A={} AXIS_B={}", first_axis_value, second_axis_value);
    variant.GlobalBindings.push_back(MakeSharedBinding());
    variant.EntryPoints.push_back(
        MakeEntryPoint(k_ActiveEntryPoint, std::format("// AXIS_A is {}\n", first_axis_value)));
    variant.EntryPoints.push_back(MakeEntryPoint(k_InertEntryPoint, "// this text never changes\n"));
    return variant;
}

/** One entry point, so the usage mask is the same on every layout. The shared layout claim is only
 * interesting when the layouts can actually be equal, and today the mask is part of the layout key. */
CompiledVariant MakeSingleEntryPointVariant(uint32_t index, bool first_axis_value)
{
    CompiledVariant variant;
    variant.VariantIndex = index;
    variant.VariantSuffix = std::format("_{}", first_axis_value);
    variant.VariantDescription = std::format("AXIS_A={}", first_axis_value);
    variant.GlobalBindings.push_back(MakeSharedBinding());
    variant.EntryPoints.push_back(
        MakeEntryPoint(k_ActiveEntryPoint, std::format("// AXIS_A is {}\n", first_axis_value)));
    return variant;
}

PermutationAssignment MakeAssignment(const PermutationAxis& first_axis,
                                     const PermutationAxis& second_axis,
                                     bool first_axis_value,
                                     bool second_axis_value)
{
    return PermutationAssignment{ PermutationBinding{ &first_axis, PermutationValue{ first_axis_value } },
                                  PermutationBinding{ &second_axis, PermutationValue{ second_axis_value } } };
}

CookedModule BuildModule(const PermutationSpace& space, bool dedupe_enabled)
{
    CookedModule module;
    module.Name = "InfluenceModule";
    module.Space = &space;
    module.SpaceSize = 4u;
    module.EntryPoints.push_back(
        LibraryEntryPoint{ .Name = std::string{ k_ActiveEntryPoint }, .Stage = ShaderStageKind::Compute });
    module.EntryPoints.push_back(
        LibraryEntryPoint{ .Name = std::string{ k_InertEntryPoint }, .Stage = ShaderStageKind::Compute });

    if (!dedupe_enabled)
    {
        module.SourceInterner.Disable();
        module.LayoutInterner.Disable();
        module.RasterInterner.Disable();
    }

    uint32_t index = 0u;
    for (const bool firstAxisValue : { false, true })
    {
        for (const bool secondAxisValue : { false, true })
        {
            const CompiledVariant variant = MakeVariant(index, firstAxisValue, secondAxisValue);
            const PermutationAssignment canonical =
                MakeAssignment(*space[0], *space[1], firstAxisValue, secondAxisValue);

            const CookResult<void> appended = AppendVariantToModule(module, variant, canonical);
            if (!appended)
            {
                module.Variants.clear();
                return module;
            }

            ++index;
        }
    }

    FreezeModuleTables(module);
    return module;
}

CookedModule BuildSingleEntryPointModule(const PermutationSpace& space, bool dedupe_enabled)
{
    CookedModule module;
    module.Name = "SharedLayoutModule";
    module.Space = &space;
    module.SpaceSize = 2u;
    module.EntryPoints.push_back(
        LibraryEntryPoint{ .Name = std::string{ k_ActiveEntryPoint }, .Stage = ShaderStageKind::Compute });

    if (!dedupe_enabled)
    {
        module.SourceInterner.Disable();
        module.LayoutInterner.Disable();
        module.RasterInterner.Disable();
    }

    uint32_t index = 0u;
    for (const bool firstAxisValue : { false, true })
    {
        const CompiledVariant variant = MakeSingleEntryPointVariant(index, firstAxisValue);
        const PermutationAssignment canonical =
            PermutationAssignment{ PermutationBinding{ space[0], PermutationValue{ firstAxisValue } } };

        const CookResult<void> appended = AppendVariantToModule(module, variant, canonical);
        if (!appended)
        {
            module.Variants.clear();
            return module;
        }

        ++index;
    }

    FreezeModuleTables(module);
    return module;
}

AxisInfluence InfluenceOf(const ModuleInfluence& influence,
                          std::string_view entry_point_name,
                          size_t axis_index)
{
    for (const EntryPointInfluence& entry : influence.EntryPoints)
    {
        if (entry.EntryPointName == entry_point_name && axis_index < entry.Axes.size())
        {
            return entry.Axes[axis_index];
        }
    }

    return AxisInfluence::Invalid;
}

/** If both arms filled the tables the same way, the comparison below proves nothing. */
void CheckTheTwoArmsReallyDiffer(lodestone::tests::TestRunner& runner,
                                 const CookedModule& deduped,
                                 const CookedModule& raw)
{
    runner.BeginSection("the two arms differ in cost");

    runner.Check(deduped.Variants.size() == 4u && raw.Variants.size() == 4u, "both arms hold every variant");
    runner.Check(deduped.Sources.size() == 3u, "dedup collapses eight artifacts onto three unique texts");
    runner.Check(raw.Sources.size() == 8u, "no-dedupe gives each of the eight artifacts its own entry");
    runner.Check(deduped.Layouts.size() == 2u,
                 "the two entry points bind one resource at one place, but the usage mask is part of "
                 "the layout key today, so the table holds one entry for each of them. Phase D step "
                 "D8b takes the mask out of the key and this becomes 1");
    runner.Check(raw.Layouts.size() == 8u, "no-dedupe gives each of the eight layouts its own entry");
}

void CheckInfluenceAgrees(lodestone::tests::TestRunner& runner,
                          const CookedModule& deduped,
                          const CookedModule& raw)
{
    runner.BeginSection("influence does not depend on dedup");

    const ModuleInfluence dedupedInfluence = ComputeAxisInfluence(deduped);
    const ModuleInfluence rawInfluence = ComputeAxisInfluence(raw);

    runner.Check(InfluenceOf(dedupedInfluence, k_ActiveEntryPoint, 0u) == AxisInfluence::Active,
                 "with dedup on, the axis the shader reads is Active");
    runner.Check(InfluenceOf(rawInfluence, k_ActiveEntryPoint, 0u) == AxisInfluence::Active,
                 "with dedup off, the axis the shader reads is still Active");

    runner.Check(InfluenceOf(dedupedInfluence, k_ActiveEntryPoint, 1u) == AxisInfluence::Inert,
                 "with dedup on, the axis the shader ignores is Inert");
    runner.Check(InfluenceOf(rawInfluence, k_ActiveEntryPoint, 1u) == AxisInfluence::Inert,
                 "with dedup off, the axis the shader ignores is still Inert. This is the check that "
                 "an index comparison fails");

    runner.Check(InfluenceOf(dedupedInfluence, k_InertEntryPoint, 0u) == AxisInfluence::Inert &&
                     InfluenceOf(dedupedInfluence, k_InertEntryPoint, 1u) == AxisInfluence::Inert,
                 "with dedup on, an entry point that reads no axis is inert on both");
    runner.Check(InfluenceOf(rawInfluence, k_InertEntryPoint, 0u) == AxisInfluence::Inert &&
                     InfluenceOf(rawInfluence, k_InertEntryPoint, 1u) == AxisInfluence::Inert,
                 "with dedup off, an entry point that reads no axis is inert on both");

    runner.Check(dedupedInfluence.EntryPoints.size() == rawInfluence.EntryPoints.size(),
                 "both arms measure the same entry points");
}

void CheckSharedLayoutAgrees(lodestone::tests::TestRunner& runner, const PermutationSpace& space)
{
    runner.BeginSection("the shared layout claim does not depend on dedup");

    const CookedModule deduped = BuildSingleEntryPointModule(space, true);
    const CookedModule raw = BuildSingleEntryPointModule(space, false);

    runner.Check(deduped.Layouts.size() == 1u && raw.Layouts.size() == 2u,
                 "the two arms hold the same one layout in a different number of entries");
    runner.Check(AllVariantsShareOneLayout(deduped), "with dedup on, every permutation shares one layout");
    runner.Check(AllVariantsShareOneLayout(raw),
                 "with dedup off, every permutation still shares one layout, because the claim is "
                 "about the content and not about the table size");
}

/** The claim must be false when it is false, or it says nothing when it is true. */
void CheckSharedLayoutRejectsADifference(lodestone::tests::TestRunner& runner, const PermutationSpace& space)
{
    runner.BeginSection("the shared layout claim is falsifiable");

    CookedModule module = BuildSingleEntryPointModule(space, true);
    runner.Check(AllVariantsShareOneLayout(module), "the module starts with one shared layout");

    ShaderLayout differentLayout = module.Layouts.front();
    differentLayout.front().Binding = 7u;
    module.Layouts.push_back(std::move(differentLayout));
    module.Variants.front().LayoutIndices.front() = static_cast<uint32_t>(module.Layouts.size() - 1u);

    runner.Check(!AllVariantsShareOneLayout(module), "one variant with a different layout ends the claim");
}

} // namespace

int main()
{
    lodestone::tests::TestRunner runner{ "DedupeInfluence" };

    const PermutationAxis firstAxis = MakeBoolAxis("AXIS_A");
    const PermutationAxis secondAxis = MakeBoolAxis("AXIS_B");
    const PermutationSpace space{ &firstAxis, &secondAxis };

    const CookedModule deduped = BuildModule(space, true);
    const CookedModule raw = BuildModule(space, false);

    CheckTheTwoArmsReallyDiffer(runner, deduped, raw);
    CheckInfluenceAgrees(runner, deduped, raw);
    CheckSharedLayoutAgrees(runner, space);
    CheckSharedLayoutRejectsADifference(runner, space);

    return runner.Report();
}
