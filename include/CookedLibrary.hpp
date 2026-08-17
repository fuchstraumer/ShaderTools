#pragma once
#ifndef LODESTONE_SHADER_COOKER_COOKED_LIBRARY_HPP
#define LODESTONE_SHADER_COOKER_COOKED_LIBRARY_HPP
#include "ContentHash.hpp"
#include "ContentInterner.hpp"
#include "CookerErrors.hpp"
#include "PermutationSpace.hpp"
#include "ShaderDataSchema.hpp"
#include "ShaderLibraryTypes.hpp"
#include <cstdint>
#include <string>
#include <string_view>
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
namespace lodestone
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

/** @brief The hash that generated a table, whether or not it used dedupe logic,
 *  and the results of the process (regardless of if it ran dedupe or not */
struct TableStatistics
{
    std::string_view HashName;
    bool DedupeEnabled{ true };
    InternerStatistics Interning;
};

/** @brief An interned module is procedurally built by adding variants as they arrive, and represents
 *  the deduplicated (if enabled) contents of a Slang module bundled together for the final cooking
 *  stage to process as it sees fit. This object actually *does* the interning piece by piece, as 
 *  compared to `CookedModule` which holds the completed results from this processing.
 * 
 * @note As of now, there is no `InternedLibrary`: interning is a per-module operation for now.
 * In the future, we could intern libraries - but those would be most efficient running on interned
 * modules, anyways. */
struct InternedModule
{
    std::string Name;
    const PermutationSpace* Space{ nullptr };
    uint32_t SpaceSize{ 0u };
    std::vector<LibraryEntryPoint> EntryPoints;
    std::vector<LibraryVariant> Variants;

    ContentInterner<std::string> SourceInterner{ &HashSourcePayload, "fnv1a-64" };
    ContentInterner<ShaderLayout> LayoutInterner{ &HashLayoutPayload, "fnv1a-64" };
    ContentInterner<ReflectedRasterState> RasterInterner{ &HashRasterPayload, "fnv1a-64" };
};

/**@brief Interned tables and information about how efficiently they were built. We store these 
 * separately as they collapse at very different rates, so it's worth having insight into each.
 * This object holds the results of the interning process, but doesn't do it itself. */
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

    TableStatistics SourceTable;
    TableStatistics LayoutTable;
    TableStatistics RasterTable;
};

/** @brief Temp. Slang is designed for big libraries of shaders, but for now we're trying
 *  to ensure core module handling is robust and modular: most Library operations will just
 *  be further fold/combine operations over the datastructures we already have. */
struct CookedLibrary
{
    std::vector<CookedModule> Modules;
};

/** Adds one compiled variant to the module, interning each source, layout, and raster state. */
CookResult<void> AppendVariantToModule(InternedModule& module,
                                       const CompiledVariant& variant,
                                       const PermutationAssignment& canonical);

/**@brief "Freezes" the module by *consuming* `InternedModule`. CookedModule takes the results, gathering
 * all the data so far in one place. The intent was that CookedModule is a bundle of data, it doesn't hold
 * systems that build or modify that data. */
CookedModule FreezeModuleTables(InternedModule&& interned);

/** Resolves what a caller would get back for one entry point of one variant. The round-trip check
 * compares this against the text the compiler produced. */
std::string_view ResolveSource(const CookedModule& module,
                               const LibraryVariant& variant,
                               size_t entry_point_index) noexcept;

} // namespace lodestone

#endif // !LODESTONE_SHADER_COOKER_COOKED_LIBRARY_HPP
