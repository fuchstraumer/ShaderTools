#pragma once
#ifndef VELOX_SHADER_COOKER_COOKED_LIBRARY_HPP
#define VELOX_SHADER_COOKER_COOKED_LIBRARY_HPP
#include "ContentInterner.hpp"
#include "PermutationSpace.hpp"
#include "ShaderDataSchema.hpp"
#include <string>
#include <vector>

/** The frozen model every emitter reads.
 *
 * One cook builds this once. The C++ emitter, the binary manifest emitter, and the dedup report all
 * read the same object, so two artifacts of one cook cannot describe different shaders.
 *
 * A variant does not hold its source text. It holds an index into the module's source table. Today
 * that table has one entry for each artifact, so the indices are an identity mapping. Dedup replaces
 * how the table gets filled, and nothing else changes: the emitters already read through the index.
 */
namespace velox::cooker
{

struct LibraryEntryPoint
{
    std::string Name;
    ShaderStageKind Stage{ ShaderStageKind::Invalid };
};

/** Hashes one WGSL source. The interner compares bytes afterwards, so this only picks the bucket. */
ContentHashValue HashSourcePayload(const std::string& source) noexcept;

/** Hashes one binding table field by field. Every field that the graph reads takes part, so two
 * layouts that differ only in a derived size stay separate. */
ContentHashValue HashLayoutPayload(const std::vector<ReflectedBinding>& layout) noexcept;

/** Hashes one raster state. A compute entry point gives an empty state, so every compute module has
 * exactly one raster entry and the table costs almost nothing. */
ContentHashValue HashRasterPayload(const ReflectedRasterState& raster) noexcept;

/** One (module, permutation) pair. The vectors run parallel to CookedModule::EntryPoints. */
struct LibraryVariant
{
    uint32_t Index{ 0u };
    std::string Suffix;
    std::string Description;
    PermutationAssignment Canonical;
    std::vector<uint32_t> SourceIndices;
    std::vector<uint32_t> LayoutIndices;
    std::vector<uint32_t> RasterIndices;
    std::vector<WorkgroupSize> Workgroups;
};

using ShaderLayout = std::vector<ReflectedBinding>;

/** Sources and layouts intern separately. They collapse at very different rates, and one number for
 * both would hide the information in each. */
struct CookedModule
{
    std::string Name;
    const PermutationSpace* Space{ nullptr };
    /** @brief Size of the dense index range, holes included. */
    uint32_t SpaceSize{ 0u };
    std::vector<LibraryEntryPoint> EntryPoints;
    std::vector<std::string> Sources;
    std::vector<ShaderLayout> Layouts;
    std::vector<ReflectedRasterState> RasterStates;
    std::vector<LibraryVariant> Variants;

    ContentInterner<std::string> SourceInterner{ &HashSourcePayload, "fnv1a-64" };
    ContentInterner<ShaderLayout> LayoutInterner{ &HashLayoutPayload, "fnv1a-64" };
    ContentInterner<ReflectedRasterState> RasterInterner{ &HashRasterPayload, "fnv1a-64" };
};

struct CookedLibrary
{
    std::vector<CookedModule> Modules;
};

/** Adds one compiled variant to the module. It appends every source and layout, so each artifact
 * gets its own index. This is the identity mapping that dedup later collapses. */
CookResult<void> AppendVariantToModule(CookedModule& module,
                                       const CompiledVariant& variant,
                                       const PermutationAssignment& canonical);

/** Copies the interned tables into the module, once every variant is in. */
void FreezeModuleTables(CookedModule& module);

/** Resolves what a caller would get back for one entry point of one variant. The round-trip check
 * compares this against the text the compiler produced. */
std::string_view ResolveSource(const CookedModule& module,
                               const LibraryVariant& variant,
                               size_t entry_point_index) noexcept;

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_COOKED_LIBRARY_HPP
