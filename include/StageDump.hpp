#pragma once
#ifndef LODESTONE_SHADER_COOKER_STAGE_DUMP_HPP
#define LODESTONE_SHADER_COOKER_STAGE_DUMP_HPP
#include "CookedLibrary.hpp"
#include "CookerOptions.hpp"
#include "PermutationSpace.hpp"
#include "RawLibrary.hpp"
#include <string>
#include <string_view>

/** Writes one point between two stages as JSON, so a change between two stages is a diff and not a
 * search.
 *
 * A dump goes through the `OutputSink` like every other artifact. Three things follow from that, and
 * all three are the reason for it:
 *
 * - `MemoryOutputSink` captures a dump, so a test reads one without touching a disk.
 * - `--verify-deterministic` compares every artifact the sink holds, so it compares the dumps. An
 *   unordered container that reaches a dump then fails the cook.
 * - A dump file is named the way every other artifact is named.
 *
 * A dump holds no target text. The WGSL already ships in three other artifacts, and a dump that
 * repeats it is large, slow to diff, and hides the tables a reader wants. A source appears as an
 * index, a byte length, and a content hash. */
namespace lodestone
{

/** `<module>.stage-<name>.json` */
std::string MakeStageDumpFileName(std::string_view module_name, StageDumpKind kind);

/** Stage 1. The axes the module declares, and the dense index range they expand to. */
std::string DumpPermutationSpace(std::string_view module_name, const PermutationSpace& space);

/** Stage 2. Every variant identity, with both the active and the canonical assignment. */
std::string DumpVariantSet(std::string_view module_name, const VariantSet& variant_set);

/** Stage 3. Everything Slang said, with every `[vx_*]` argument still a string. A reader can see here
 * whether a wrong size came out of the compiler or out of the arithmetic that follows it. */
std::string DumpRawModule(const RawModule& module);

/** Stage 7. The frozen tables, the indices each variant keys into them with, and what each interner
 * measured while it filled them. */
std::string DumpCookedModule(const CookedModule& module);

} // namespace lodestone

#endif // !LODESTONE_SHADER_COOKER_STAGE_DUMP_HPP
