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

ContentHashValue HashResourcePayload(const ReflectedBinding& resource) noexcept
{
    ContentHashValue hash = HashFnv1a64("resource");
    hash = CombineHash(hash, HashFnv1a64(resource.Name));
    hash = CombineHash(hash, resource.Placement.index());
    hash = CombineHash(hash, GroupOf(resource));
    hash = CombineHash(hash, BindingOf(resource));
    hash = CombineHash(hash, static_cast<uint64_t>(resource.Kind));
    hash = CombineHash(hash, resource.ElementStride);
    hash = CombineHash(hash, resource.ByteSize);
    hash = CombineHash(hash, resource.ArrayCount);
    hash = CombineHash(hash, static_cast<uint64_t>(resource.Shape));
    hash = CombineHash(hash, static_cast<uint64_t>(resource.SampleType));
    hash = CombineHash(hash, static_cast<uint64_t>(resource.StorageFormat));
    hash = CombineHash(hash, static_cast<uint64_t>(resource.StorageAccess));
    hash = CombineHash(hash, static_cast<uint64_t>(resource.SamplerType));

    for (const ReflectedUniformMember& member : resource.UniformMembers)
    {
        hash = CombineHash(hash, HashFnv1a64(member.Name));
        hash = CombineHash(hash, member.Offset);
        hash = CombineHash(hash, member.Size);
        hash = CombineHash(hash, member.ArrayCount);
    }

    return hash;
}

ContentHashValue HashIndexListPayload(const std::vector<uint32_t>& indices) noexcept
{
    ContentHashValue hash = HashFnv1a64("indices");

    for (const uint32_t index : indices)
    {
        hash = CombineHash(hash, index);
    }

    return hash;
}

ContentHashValue HashFootprintListPayload(const std::vector<ResourceFootprint>& footprints) noexcept
{
    ContentHashValue hash = HashFnv1a64("footprints");

    for (const ResourceFootprint& footprint : footprints)
    {
        hash = CombineHash(hash, footprint.index());

        if (const BufferFootprint* buffer = std::get_if<BufferFootprint>(&footprint))
        {
            hash = CombineHash(hash, buffer->ElementCount);
            hash = CombineHash(hash, HashFnv1a64(buffer->Expression));
        }
        else if (const TextureFootprint* texture = std::get_if<TextureFootprint>(&footprint))
        {
            hash = CombineHash(hash, texture->ExtentX);
            hash = CombineHash(hash, texture->ExtentY);
            hash = CombineHash(hash, texture->ExtentZ);
            hash = CombineHash(hash, HashFnv1a64(texture->Expression));
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

void DisableDedupe(InternedModule& module) noexcept
{
    // todo: This is brittle and vulnerable to being missed whenever we add or change interners.
    // Can we do something smarter? (that's rhetorical. but think about it maybe)
    module.SourceInterner.Disable();
    module.ResourceInterner.Disable();
    module.ResourceListInterner.Disable();
    module.FootprintListInterner.Disable();
    module.VisibilityInterner.Disable();
    module.RasterInterner.Disable();
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

    const ProvenanceRecord variantOrigin{ .EntryPointName = {},
                                          .VariantDescription = variant.VariantDescription,
                                          .VariantIndex = variant.VariantIndex };

    // Resources are per-variant (since that's the granularity we will build resources and bind 
    // models at, not entrypoints: this is one of the advantages of our system)
    ResourceList resources;
    resources.reserve(variant.GlobalBindings.size());
    for (const ReflectedBinding& binding : variant.GlobalBindings)
    {
        resources.push_back(module.ResourceInterner.Intern(binding, variantOrigin).Index);
    }

    LibraryVariant record;
    record.Index = variant.VariantIndex;
    record.Suffix = variant.VariantSuffix;
    record.Description = variant.VariantDescription;
    record.Canonical = canonical;
    record.ResourceListIndex = module.ResourceListInterner.Intern(std::move(resources), variantOrigin).Index;
    record.FootprintListIndex = module.FootprintListInterner.Intern(variant.Footprints, variantOrigin).Index;
    record.SourceIndices.reserve(variant.EntryPoints.size());
    record.VisibilityIndices.reserve(variant.EntryPoints.size());
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

        const InternResult visibility =
            module.VisibilityInterner.Intern(entryPoint.Reflection.UsedBindingIndices, origin);
        record.VisibilityIndices.push_back(visibility.Index);

        const InternResult raster = module.RasterInterner.Intern(entryPoint.Reflection.Raster, origin);
        record.RasterIndices.push_back(raster.Index);

        record.Workgroups.push_back(entryPoint.Reflection.Workgroup);
    }

    module.Variants.emplace_back(std::move(record));
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
    module.Resources = std::move(interned.ResourceInterner.ConsumeTable());
    module.ResourceLists = std::move(interned.ResourceListInterner.ConsumeTable());
    module.FootprintLists = std::move(interned.FootprintListInterner.ConsumeTable());
    module.VisibilityLists = std::move(interned.VisibilityInterner.ConsumeTable());
    module.RasterStates = std::move(interned.RasterInterner.ConsumeTable());

    module.SourceTable = DescribeTable(interned.SourceInterner);
    module.ResourceTable = DescribeTable(interned.ResourceInterner);
    module.ResourceListTable = DescribeTable(interned.ResourceListInterner);
    module.FootprintListTable = DescribeTable(interned.FootprintListInterner);
    module.VisibilityTable = DescribeTable(interned.VisibilityInterner);
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

ShaderLayout ResolveLayout(const CookedModule& module,
                           const LibraryVariant& variant,
                           size_t entry_point_index)
{
    // remember that visiblity indices is per-EP in this data, so if
    // input index is bigger than that list, it's not in range
    if (entry_point_index >= variant.VisibilityIndices.size() ||
        variant.ResourceListIndex >= module.ResourceLists.size() ||
        variant.FootprintListIndex >= module.FootprintLists.size())
    {
        return {};
    }

    // another indirection: visiblity lists are stored separately. EP idx just keys to that.
    // (remember, resource list size can change considerably between EPs)
    const uint32_t visibilityIndex = variant.VisibilityIndices[entry_point_index];
    if (visibilityIndex >= module.VisibilityLists.size())
    {
        return {};
    }

    const ResourceList& resources = module.ResourceLists[variant.ResourceListIndex];
    const FootprintList& footprints = module.FootprintLists[variant.FootprintListIndex];

    ShaderLayout layout;
    layout.reserve(module.VisibilityLists[visibilityIndex].size());
    // localRsrcIndex == binding of the resource in *this* entry point
    for (const uint32_t localRsrcIndex : module.VisibilityLists[visibilityIndex])
    {
        if (localRsrcIndex >= resources.size() || resources[localRsrcIndex] >= module.Resources.size())
        {
            return {};
        }

        ResourceFootprint footprint = localRsrcIndex < footprints.size() ? footprints[localRsrcIndex] : ResourceFootprint{};
        layout.emplace_back(module.Resources[resources[localRsrcIndex]], footprint);
    }

    return layout;
}

} // namespace lodestone
