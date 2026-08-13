#pragma once
#ifndef VELOX_SHADER_COOKER_COOKED_LIBRARY_HPP
#define VELOX_SHADER_COOKER_COOKED_LIBRARY_HPP
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

/** One (module, permutation) pair. The vectors run parallel to CookedModule::EntryPoints. */
struct LibraryVariant
{
    uint32_t Index{ 0u };
    std::string Suffix;
    std::string Description;
    PermutationAssignment Canonical;
    std::vector<uint32_t> SourceIndices;
    std::vector<uint32_t> LayoutIndices;
    std::vector<WorkgroupSize> Workgroups;
};

struct CookedModule
{
    std::string Name;
    const PermutationSpace* Space{ nullptr };
    /** @brief Size of the dense index range, holes included. */
    uint32_t SpaceSize{ 0u };
    std::vector<LibraryEntryPoint> EntryPoints;
    std::vector<std::string> Sources;
    std::vector<std::vector<ReflectedBinding>> Layouts;
    std::vector<LibraryVariant> Variants;
};

struct CookedLibrary
{
    std::vector<CookedModule> Modules;
};

/** Adds one compiled variant to the module. It appends every source and layout, so each artifact
 * gets its own index. This is the identity mapping that dedup later collapses. */
CookResult<void> AppendVariantToModule(CookedModule& module, const CompiledVariant& variant);

/** Resolves what a caller would get back for one entry point of one variant. The round-trip check
 * compares this against the text the compiler produced. */
std::string_view ResolveSource(const CookedModule& module,
                               const LibraryVariant& variant,
                               size_t entry_point_index) noexcept;

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_COOKED_LIBRARY_HPP
