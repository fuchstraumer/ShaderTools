#include "CookedLibrary.hpp"
#include "ContentHash.hpp"
#include "ContentInterner.hpp"
#include "CookerErrors.hpp"
#include "PermutationSpace.hpp"
#include "ShaderDataSchema.hpp"

#include <cstdint>
#include <cstdio>
#include <expected>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lodestone
{

ContentHashValue HashSourcePayload(const std::string& source) noexcept
{
    return HashFnv1a64(source);
}

ContentHashValue HashLayoutPayload(const std::vector<ReflectedBinding>& layout) noexcept
{
    ContentHashValue hash = HashFnv1a64("layout");

    for (const ReflectedBinding& binding : layout)
    {
        hash = CombineHash(hash, HashFnv1a64(binding.Name));
        hash = CombineHash(hash, binding.Group);
        hash = CombineHash(hash, binding.Binding);
        hash = CombineHash(hash, static_cast<uint64_t>(binding.Kind));
        hash = CombineHash(hash, binding.EntryPointUsageMask);
        hash = CombineHash(hash, binding.ElementStride);
        hash = CombineHash(hash, binding.ByteSize);
        hash = CombineHash(hash, binding.ArrayCount);
        hash = CombineHash(hash, static_cast<uint64_t>(binding.Shape));
        hash = CombineHash(hash, static_cast<uint64_t>(binding.SampleType));
        hash = CombineHash(hash, static_cast<uint64_t>(binding.StorageFormat));
        hash = CombineHash(hash, static_cast<uint64_t>(binding.StorageAccess));
        hash = CombineHash(hash, static_cast<uint64_t>(binding.SamplerType));
        hash = CombineHash(hash, binding.Derived.ElementCount);
        hash = CombineHash(hash, binding.Derived.ExtentX);
        hash = CombineHash(hash, binding.Derived.ExtentY);
        hash = CombineHash(hash, binding.Derived.ExtentZ);

        for (const ReflectedUniformMember& member : binding.UniformMembers)
        {
            hash = CombineHash(hash, HashFnv1a64(member.Name));
            hash = CombineHash(hash, member.Offset);
            hash = CombineHash(hash, member.Size);
            hash = CombineHash(hash, member.ArrayCount);
        }
    }

    return hash;
}

ContentHashValue HashRasterPayload(const ReflectedRasterState& raster) noexcept
{
    ContentHashValue hash = HashFnv1a64("raster");

    for (const ReflectedVertexInput& input : raster.VertexInputs)
    {
        hash = CombineHash(hash, HashFnv1a64(input.SemanticName));
        hash = CombineHash(hash, input.SemanticIndex);
        hash = CombineHash(hash, input.Location);
        hash = CombineHash(hash, static_cast<uint64_t>(input.ScalarType));
        hash = CombineHash(hash, input.ComponentCount);
    }

    for (const ReflectedColorTarget& target : raster.ColorTargets)
    {
        hash = CombineHash(hash, target.Location);
        hash = CombineHash(hash, static_cast<uint64_t>(target.ScalarType));
        hash = CombineHash(hash, target.ComponentCount);
    }

    return CombineHash(hash, raster.WritesFragDepth ? 1u : 0u);
}

CookResult<void> AppendVariantToModule(InternedModule& module,
                                       const CompiledVariant& variant,
                                       const PermutationAssignment& canonical)
{
    if (variant.EntryPoints.size() != module.EntryPoints.size())
    {
        std::println(stderr,
                     "[shader_cooker] variant [{}] has {} entrypoints, but module {} declares {}",
                     variant.VariantDescription,
                     variant.EntryPoints.size(),
                     module.Name,
                     module.EntryPoints.size());
        return std::unexpected(CookError::ReflectionMismatch);
    }

    LibraryVariant record;
    record.Index = variant.VariantIndex;
    record.Suffix = variant.VariantSuffix;
    record.Description = variant.VariantDescription;
    record.Canonical = canonical;
    record.SourceIndices.reserve(variant.EntryPoints.size());
    record.LayoutIndices.reserve(variant.EntryPoints.size());
    record.RasterIndices.reserve(variant.EntryPoints.size());
    record.Workgroups.reserve(variant.EntryPoints.size());
    // we could probably make this more efficient in the future, it'll be moving a good bit of data 
    // around as variant size grows
    // todo-ship: once we have a better logger with levels, it'd be interesting to see how uniqueness count
    // changes and grows per variant
    for (size_t entryPointIndex = 0u; entryPointIndex < variant.EntryPoints.size(); ++entryPointIndex)
    {
        const CompiledEntryPoint& entryPoint = variant.EntryPoints[entryPointIndex];
        const ProvenanceRecord origin{ .EntryPointName = entryPoint.Name,
                                       .VariantDescription = variant.VariantDescription,
                                       .VariantIndex = variant.VariantIndex };

        const InternResult source = module.SourceInterner.Intern(entryPoint.Code, origin);
        record.SourceIndices.push_back(source.Index);

        const InternResult layout =
            module.LayoutInterner.Intern(BuildEntryPointLayout(variant, entryPointIndex), origin);
        record.LayoutIndices.push_back(layout.Index);

        const InternResult raster = module.RasterInterner.Intern(entryPoint.Reflection.Raster, origin);
        record.RasterIndices.push_back(raster.Index);

        record.Workgroups.push_back(entryPoint.Reflection.Workgroup);
    }

    module.Variants.push_back(std::move(record));
    return {};
}

namespace
{

    template<typename PayloadType>
    TableStatistics DescribeTable(const ContentInterner<PayloadType>& interner)
    {
        return TableStatistics{ .HashName = interner.HashName(),
                                .DedupeEnabled = interner.IsEnabled(),
                                .Interning = interner.Statistics() };
    }

} // namespace

CookedModule FreezeModuleTables(InternedModule&& interned)
{
    CookedModule module;
    module.Name = std::move(interned.Name);
    module.Space = interned.Space;
    module.SpaceSize = interned.SpaceSize;
    module.EntryPoints = std::move(interned.EntryPoints);
    module.Variants = std::move(interned.Variants);

    module.Sources = std::move(interned.SourceInterner.ConsumeTable());
    module.Layouts = std::move(interned.LayoutInterner.ConsumeTable());
    module.RasterStates = std::move(interned.RasterInterner.ConsumeTable());

    module.SourceTable = DescribeTable(interned.SourceInterner);
    module.LayoutTable = DescribeTable(interned.LayoutInterner);
    module.RasterTable = DescribeTable(interned.RasterInterner);

    return module;
}

std::string_view ResolveSource(const CookedModule& module,
                               const LibraryVariant& variant,
                               size_t entry_point_index) noexcept
{
    if (entry_point_index >= variant.SourceIndices.size())
    {
        return {};
    }

    const uint32_t sourceIndex = variant.SourceIndices[entry_point_index];
    if (sourceIndex >= module.Sources.size())
    {
        return {};
    }

    return module.Sources[sourceIndex];
}

} // namespace lodestone
