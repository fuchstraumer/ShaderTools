#include "TestHarness.hpp"

#include "CookedLibrary.hpp"
#include "CookerOptions.hpp"
#include "PermutationSpace.hpp"
#include "ShaderDataSchema.hpp"
#include "ShaderLibraryTypes.hpp"
#include "StageDump.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace lodestone;

namespace
{

/** The space dump for this axis is written out in full below. Keep the axis small, because the value
 * of that golden is that a person can read it and see the whole format at once. */
PermutationAxis MakeBoolAxis(std::string name)
{
    PermutationAxis axis;
    axis.Name = std::move(name);
    axis.Values = { PermutationValue{ false }, PermutationValue{ true } };
    return axis;
}

CompiledVariant MakeVariant(uint32_t index, std::string suffix, std::string code)
{
    ReflectedBinding binding;
    binding.Name = "Waves";
    binding.Group = 0u;
    binding.Binding = 1u;
    binding.Kind = BindingKind::StorageBuffer;
    binding.EntryPointUsageMask = 1u;
    binding.ElementStride = 16u;
    binding.ArrayCount = 1u;
    binding.Shape = ResourceShape::Buffer;
    binding.Derived.Expression = "IFFT_SIZE * 4";
    binding.Derived.ElementCount = 1024u;
    binding.Derived.HasElementCount = true;

    CompiledEntryPoint entryPoint;
    entryPoint.Name = "MainCS";
    entryPoint.VariantSuffix = suffix;
    entryPoint.Code = std::move(code);
    entryPoint.Reflection.Name = "MainCS";
    entryPoint.Reflection.Stage = ShaderStageKind::Compute;
    entryPoint.Reflection.Workgroup = WorkgroupSize{ .X = 64u, .Y = 1u, .Z = 1u };
    entryPoint.Reflection.Bindings.push_back(binding);

    CompiledVariant variant;
    variant.VariantIndex = index;
    variant.VariantSuffix = suffix;
    variant.VariantDescription = "USE_FOO=" + std::string{ index == 0u ? "false" : "true" };
    variant.EntryPoints.push_back(std::move(entryPoint));
    return variant;
}

/** Two variants with different text and one shared layout. The cooked dump must therefore report two
 * sources and one layout, which is the collapse the interner performed. */
CookedModule BuildTinyModule()
{
    CookedModule module;
    module.Name = "TinyModule";
    module.SpaceSize = 2u;
    module.EntryPoints.push_back(LibraryEntryPoint{ .Name = "MainCS", .Stage = ShaderStageKind::Compute });

    const std::array<CompiledVariant, 2u> variants{ MakeVariant(0u, "_USE_FOO_false", "// variant zero\n"),
                                                    MakeVariant(1u, "_USE_FOO_true", "// variant one\n") };

    for (const CompiledVariant& variant : variants)
    {
        const CookResult<void> appended = AppendVariantToModule(module, variant, PermutationAssignment{});
        if (!appended)
        {
            module.Variants.clear();
            return module;
        }
    }

    FreezeModuleTables(module);
    return module;
}

bool Contains(std::string_view text, std::string_view needle) noexcept
{
    return text.find(needle) != std::string_view::npos;
}

void CheckStageNames(lodestone::tests::TestRunner& runner)
{
    runner.BeginSection("stage names");

    runner.Check(ParseStageDumpKind("space") == StageDumpKind::Space, "space parses");
    runner.Check(ParseStageDumpKind("variants") == StageDumpKind::Variants, "variants parses");
    runner.Check(ParseStageDumpKind("raw") == StageDumpKind::Raw, "raw parses");
    runner.Check(ParseStageDumpKind("resolved") == StageDumpKind::Resolved, "resolved parses");
    runner.Check(ParseStageDumpKind("interned") == StageDumpKind::Interned, "interned parses");
    runner.Check(ParseStageDumpKind("cooked") == StageDumpKind::Cooked, "cooked parses");
    runner.Check(ParseStageDumpKind("all") == StageDumpKind::Invalid,
                 "all is not a kind, it is a shorthand the parser expands");
    runner.Check(ParseStageDumpKind("Cooked") == StageDumpKind::Invalid, "a name is case sensitive");
    runner.Check(ParseStageDumpKind("") == StageDumpKind::Invalid, "an empty name is rejected");

    runner.Check(ToString(StageDumpKind::Cooked) == "cooked", "a kind names itself the way it parses");
    runner.Check(MakeStageDumpFileName("OceanFft", StageDumpKind::Cooked) == "OceanFft.stage-cooked.json",
                 "the artifact name carries the module and the stage");
}

void CheckStageBits(lodestone::tests::TestRunner& runner)
{
    runner.BeginSection("stage bits");

    runner.Check(StageDumpBit(StageDumpKind::Invalid) == 0u, "the invalid kind claims no bit");
    runner.Check(StageDumpBit(StageDumpKind::Space) != StageDumpBit(StageDumpKind::Cooked),
                 "two kinds take two bits");

    CookerOptions options;
    runner.Check(!IsStageDumpRequested(options, StageDumpKind::Cooked), "no dump is requested by default");

    options.DumpStageMask = AllStageDumpBits();
    runner.Check(IsStageDumpRequested(options, StageDumpKind::Space) &&
                     IsStageDumpRequested(options, StageDumpKind::Variants) &&
                     IsStageDumpRequested(options, StageDumpKind::Raw) &&
                     IsStageDumpRequested(options, StageDumpKind::Resolved) &&
                     IsStageDumpRequested(options, StageDumpKind::Interned) &&
                     IsStageDumpRequested(options, StageDumpKind::Cooked),
                 "all requests every kind");
    runner.Check(!IsStageDumpRequested(options, StageDumpKind::Invalid),
                 "all does not request the invalid kind");
}

void CheckCommandLine(lodestone::tests::TestRunner& runner)
{
    runner.BeginSection("command line");

    const std::array<std::string_view, 4u> accepted{ "-o", "out.hpp", "--dump-stage=cooked", "a.slang" };
    const CookResult<CookerOptions> parsed = ParseCommandLine(accepted);
    runner.Check(parsed.has_value(), "--dump-stage=cooked parses");
    if (parsed)
    {
        runner.Check(IsStageDumpRequested(parsed.value(), StageDumpKind::Cooked),
                     "--dump-stage=cooked requests the cooked dump");
        runner.Check(!IsStageDumpRequested(parsed.value(), StageDumpKind::Space),
                     "--dump-stage=cooked requests nothing else");
    }

    const std::array<std::string_view, 5u> repeated{
        "-o", "out.hpp", "--dump-stage=space", "--dump-stage=variants", "a.slang"
    };
    const CookResult<CookerOptions> twice = ParseCommandLine(repeated);
    runner.Check(twice.has_value(), "the flag repeats");
    if (twice)
    {
        runner.Check(IsStageDumpRequested(twice.value(), StageDumpKind::Space) &&
                         IsStageDumpRequested(twice.value(), StageDumpKind::Variants),
                     "a repeated flag adds to the mask instead of replacing it");
    }

    const std::array<std::string_view, 4u> everything{ "-o", "out.hpp", "--dump-stage=all", "a.slang" };
    const CookResult<CookerOptions> all = ParseCommandLine(everything);
    runner.Check(all.has_value() && all.value().DumpStageMask == AllStageDumpBits(),
                 "--dump-stage=all sets every bit");

    const std::array<std::string_view, 4u> bogus{ "-o", "out.hpp", "--dump-stage=nonsense", "a.slang" };
    const CookResult<CookerOptions> rejected = ParseCommandLine(bogus);
    runner.Check(!rejected.has_value() && rejected.error() == CookError::MalformedArgument,
                 "an unknown stage name fails the command line");
}

/** The one golden literal in this file. It pins the JSON shape, the key names, the key order, and the
 * four-space indent, so a change to any of those fails here rather than in a 105-variant diff. */
void CheckSpaceDump(lodestone::tests::TestRunner& runner)
{
    runner.BeginSection("space dump");

    const PermutationAxis axis = MakeBoolAxis("USE_FOO");
    const PermutationSpace space{ &axis };

    const std::string expected = R"({
    "stage": "space",
    "module": "TinyModule",
    "axisCount": 1,
    "spaceSize": 2,
    "axes": [
        {
            "name": "USE_FOO",
            "values": [
                "false",
                "true"
            ],
            "parent": null,
            "requiredParentValue": null
        }
    ]
})";

    const std::string dump = DumpPermutationSpace("TinyModule", space);
    runner.Check(dump == expected, "the space dump matches the golden text");
    if (dump != expected)
    {
        std::printf("--- expected ---\n%s\n--- actual ---\n%s\n", expected.c_str(), dump.c_str());
    }
}

void CheckDependentAxisDump(lodestone::tests::TestRunner& runner)
{
    runner.BeginSection("dependent axis");

    const PermutationAxis parent = MakeBoolAxis("USE_FOO");
    PermutationAxis child = MakeBoolAxis("FOO_DETAIL");
    child.Parent = &parent;
    child.RequiredParentValue = PermutationValue{ true };

    const PermutationSpace space{ &parent, &child };
    const std::string dump = DumpPermutationSpace("TinyModule", space);

    runner.Check(Contains(dump, R"("parent": "USE_FOO")"), "a dependent axis names its parent");
    runner.Check(Contains(dump, R"("requiredParentValue": "true")"),
                 "a dependent axis states the value that enables it");
    runner.Check(Contains(dump, R"("spaceSize": 4)"),
                 "the space size counts the dense range with the holes included");
}

void CheckVariantDump(lodestone::tests::TestRunner& runner)
{
    runner.BeginSection("variants dump");

    const PermutationAxis axis = MakeBoolAxis("USE_FOO");
    const PermutationSpace space{ &axis };

    const CookResult<VariantSet> variantSet = EnumerateVariants(space);
    runner.Check(variantSet.has_value(), "the space enumerates");
    if (!variantSet)
    {
        return;
    }

    const std::string dump = DumpVariantSet("TinyModule", variantSet.value());
    runner.Check(Contains(dump, R"("stage": "variants")"), "the dump names its stage");
    runner.Check(Contains(dump, R"("variantCount": 2)"), "both variants reach the dump");
    runner.Check(Contains(dump, R"("axis": "USE_FOO")"), "an assignment names its axis");
    runner.Check(Contains(dump, R"("active")") && Contains(dump, R"("canonical")"),
                 "a variant carries both assignments, because they are two different facts");

    const std::string second = DumpVariantSet("TinyModule", variantSet.value());
    runner.Check(dump == second, "two dumps of one input agree byte for byte");
}

void CheckCookedDump(lodestone::tests::TestRunner& runner)
{
    runner.BeginSection("cooked dump");

    const CookedModule module = BuildTinyModule();
    runner.Check(module.Variants.size() == 2u, "both variants reached the tables");

    const std::string dump = DumpCookedModule(module);

    runner.Check(Contains(dump, R"("stage": "cooked")"), "the dump names its stage");
    runner.Check(Contains(dump, R"("module": "TinyModule")"), "the dump names its module");
    runner.Check(Contains(dump, R"("sourceCount": 2)"), "two different texts stay two sources");
    runner.Check(Contains(dump, R"("layoutCount": 1)"),
                 "one shared layout collapses to one entry, and the dump shows the collapse");
    runner.Check(Contains(dump, R"("rasterStateCount": 1)"), "a compute module has exactly one raster entry");

    runner.Check(Contains(dump, R"("name": "Waves")"), "a binding reaches the layout table");
    runner.Check(Contains(dump, R"("expression": "IFFT_SIZE * 4")"),
                 "the size expression survives, because a diagnostic must quote what the author wrote");
    runner.Check(Contains(dump, R"("elementCount": 1024)"), "the evaluated size reaches the dump");
    runner.Check(Contains(dump, R"("hashName": "fnv1a-64")"),
                 "the dump names the hash, because the name reaches the output");
    runner.Check(Contains(dump, R"("hashCollisions": 0)"), "the interner reports its collision count");

    runner.Check(!Contains(dump, "// variant zero"),
                 "no target text reaches the dump, because it already ships in three other artifacts");
    runner.Check(Contains(dump, R"("byteLength": 16)"), "a source appears as a length");
    runner.Check(Contains(dump, R"("contentHash")"), "a source appears as a hash");

    const std::string second = DumpCookedModule(module);
    runner.Check(dump == second, "two dumps of one module agree byte for byte");
}

/** A dump that cannot tell two modules apart is worth nothing as a regression harness, so prove it
 * moves when the model moves. */
void CheckCookedDumpDetectsChange(lodestone::tests::TestRunner& runner)
{
    runner.BeginSection("cooked dump sensitivity");

    const CookedModule original = BuildTinyModule();

    CookedModule changed = BuildTinyModule();
    runner.Check(!changed.Sources.empty(), "the module has a source to change");
    if (changed.Sources.empty())
    {
        return;
    }

    changed.Sources[0].push_back('X');
    runner.Check(DumpCookedModule(original) != DumpCookedModule(changed),
                 "one changed byte of source moves the dump");

    CookedModule reordered = BuildTinyModule();
    runner.Check(reordered.Variants.size() == 2u, "the module has two variants to reorder");
    if (reordered.Variants.size() != 2u)
    {
        return;
    }

    std::swap(reordered.Variants[0], reordered.Variants[1]);
    runner.Check(DumpCookedModule(original) != DumpCookedModule(reordered),
                 "variant order is part of the dump, so an unordered container cannot hide in it");
}

} // namespace

int main()
{
    lodestone::tests::TestRunner runner{ "StageDump" };

    CheckStageNames(runner);
    CheckStageBits(runner);
    CheckCommandLine(runner);
    CheckSpaceDump(runner);
    CheckDependentAxisDump(runner);
    CheckVariantDump(runner);
    CheckCookedDump(runner);
    CheckCookedDumpDetectsChange(runner);

    return runner.Report();
}
