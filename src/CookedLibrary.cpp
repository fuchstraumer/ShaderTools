#include "CookedLibrary.hpp"
#include <print>

namespace velox::cooker
{

CookResult<void> AppendVariantToModule(CookedModule& module, const CompiledVariant& variant)
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
    record.SourceIndices.reserve(variant.EntryPoints.size());
    record.LayoutIndices.reserve(variant.EntryPoints.size());
    record.Workgroups.reserve(variant.EntryPoints.size());

    for (const CompiledEntryPoint& entryPoint : variant.EntryPoints)
    {
        record.SourceIndices.push_back(static_cast<uint32_t>(module.Sources.size()));
        module.Sources.push_back(entryPoint.Code);

        record.LayoutIndices.push_back(static_cast<uint32_t>(module.Layouts.size()));
        module.Layouts.push_back(entryPoint.Reflection.Bindings);

        record.Workgroups.push_back(entryPoint.Reflection.Workgroup);
    }

    module.Variants.push_back(std::move(record));
    return {};
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

} // namespace velox::cooker
