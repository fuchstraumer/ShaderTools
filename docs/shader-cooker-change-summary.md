# Shader cooker: change summary for review

This document supports a code review of the `split-shader-branch` subtree. It tells you what the tool
does, what each commit group changed, and which sharp edges look like defects but are not.

---

## 1. What the tool does

The cooker is an offline host tool. It compiles Slang to WGSL, extracts reflection, and writes data
that a renderer reads at run time.

The pipeline in order:

1. Read a permutation space. An axis is an `extern const static` constant in the Slang source.
2. Enumerate every permutation. Fill each disabled axis with its first value.
3. Compile each permutation through Slang. Link the active axis values.
4. Extract reflection. Evaluate `[vx_*]` size attributes against the permutation values.
5. Cross-check the reflection against the emitted WGSL text.
6. Intern identical sources and identical layouts.
7. Give each permutation a dense mixed-radix index.
8. Write a C++ library, a binary manifest, and a dedup report.

The tool proves its own correctness. Exit 0 means every variant compiled, every reflection matched the
emitted WGSL, and both round trips read back the same bytes.

---

## 2. Commit map

33 commits. Read them in these groups. The messages are accurate up to `891b44f`. The last few
commits carry short messages, because they closed the transition.

### Group 1 — layout and base overhaul (`371f6bf`, `1bba390`)

`371f6bf` moved files into `include/` and `src/`. `1bba390` reshaped the driver, the options, the
errors, the output sink, and the permutation space. Treat these two as the new baseline.

### Group 2 — size expressions and attributes (`fbd05d4`, `018281e`, `c78b577`, `228a8b3`, `0c8cf9a`)

`SizeExpression.{hpp,cpp}` is a recursive descent evaluator. It names no Slang type, so it is pure and
testable.

**Read this to understand the design.** Slang cannot fold `[vx_size(IFFT_SIZE * 4)]`. An attribute
integer argument folds at compile time. The permutation constants fold at **link** time. Reflection
returns a value only for a literal. A string argument passes through unchanged, so the expression
travels as a string and the cooker evaluates it.

`c78b577` split a variant identity into `Active` and `Canonical`. `Active` drives the linker and keeps
the emitted WGSL unchanged. `Canonical` drives the index. This is why canonicalization cannot change
shader output.

### Group 3 — round trip and the frozen model (`744480c`, `9ede2c9`, `ad8e45c`, `120296d`, `0ed9887`, `b7490f2`, `7ad4cc5`, `79e2dce`, `2d31a52`)

`CookedLibrary` is the frozen model. Every emitter reads it and reaches past it never. A variant holds
indices, not text.

`ad8e45c` added the round trip check. It replays every `(module, entry point, permutation)` through the
finished tables and compares the result against the text still in memory. It runs on every cook.

### Group 4 — dedup (`557d41c`, `b445ffe`, `fc06d23`, `028a0a3`, `42281e6`, `e5e9486`, `d5a2218`, `1c69304`)

`ContentInterner` holds a vector of candidates in each bucket. Each hit compares bytes. It counts each
collision and reports it.

`fc06d23` added `ModulePolicy`: a declared variant budget, and a declared inert or active state for
each axis. A change in axis influence fails the cook and names the axis and the entry point.

`e5e9486` added `--no-dedupe` and `--verify-deterministic`.

### Group 5 — raster and the manifest (`891b44f`, `b31c6b6`, `cdec41d`, `5025793`, `93074bd`, `ee1ba5e`, `820f638`, `14968bd`, `eea6fe4`)

`ee1ba5e` added the binary manifest writer. `820f638` added raster reflection and uniform block
members. `14968bd` extended the WGSL scanner to read the address space.

---

## 3. The subtree does not build alone

**Read this before you report a broken build.** `git subtree split -P tools/shader_cooker` kept only
the cooker. The parent repository holds the rest.

These files are absent, and `CMakeLists.txt` or an `#include` still names each one:

| Missing | Named by |
|---|---|
| `src/shader/ShaderLibraryTypes.cpp` | `CMakeLists.txt`, through `${CMAKE_SOURCE_DIR}` |
| `src/shader/ShaderManifest.cpp` | `CMakeLists.txt`, through `${CMAKE_SOURCE_DIR}` |
| `include/shader/ShaderLibraryTypes.hpp` | `ShaderDataSchema.hpp` |
| `include/shader/ShaderManifest.hpp` | `ShaderManifestEmitter.cpp` |
| `include/resource/ResourceFlags.hpp` | `ShaderLibraryTypes.hpp`, for `TextureFormat` |
| `assets/shaders/VeloxAttributes.slang` | each shader that uses `[vx_element_count]` |
| `assets/shaders/compute/OceanFft.slang` | the permutation registry |
| `assets/shaders/render/VertexFormats.slang` | the raster test |
| `tests/unit_tests/SizeExpressionTests.cpp` | the size expression tests |
| `copy_slang_dlls_to_target` | `CMakeLists.txt`, a parent CMake macro |
| targets `slang`, `magic_enum::magic_enum` | `CMakeLists.txt`, parent submodules |

The target keeps the name `OceanShaderCompiler`. A rename is planned and is not part of this change.

---

## 4. Deliberate choices that look like defects

Do not report these as bugs. Each one is a decision with a reason.

**FNV-1a, not a strong hash.** The hash selects a bucket. A byte comparison decides equality. A
collision adds a second entry, and the report counts it. `ContentHashFunction` is a named function
pointer, so a replacement is one function and one name.

**`reinterpret_cast` over the manifest byte span.** The reader maps records in place and copies
nothing. `Open()` checks the magic, the version, the file size, the 8-byte alignment, and the bounds of
every section once. The accessors then stay branch-light.

**Field order in `ManifestBinding`.** The 8-byte members come first, so the record needs no padding on
any target. `static_assert` guards each record size. Do not reorder the fields.

**Empty rows in the variant tables.** A dependent axis leaves holes in the dense index range. The plan
accepts the holes and does not pay for a lookup structure. 56 slots hold 35 variants.

**String literals split at 8192 bytes.** MSVC error C2026 caps one literal at 16380 bytes. The
compiler joins the adjacent pieces, so the value does not change.

**A ternary in the generated code for a two-value boolean axis.** A `switch` over `true` and `false`
raises MSVC warning C4809.

**Duplicate work in `--verify-deterministic`.** It cooks twice on purpose and compares the artifacts.

**`ReportUndrivenExternConstants` warns and does not fail.** An `extern` constant with no axis is
legal. It keeps its declared default. `IFFT_NUM_WAVE_CASCADES` is the live example.

**The axis influence matrix compares pairs.** It is
`O(entry points x axes x variants squared)` integer compares. 35 variants make this free. A group-by
would reduce it, and no measurement asks for that yet.

---

## 5. Real defects, already known

Report these only if you find more. They are on the list.

1. **`k_UsageText` is stale.** `CookerOptions.cpp` lists neither `--no-dedupe` nor
   `--verify-deterministic`. Both work.
2. **The entry point identifier counts from one.** `EntryPointId::Invalid` is 0, so `FindSlot`
   subtracts one. A `todo-ship` in `ShaderLibraryTypes.hpp` records the fix: change to `int32_t` with
   `Invalid = -1` and count from zero.
3. **The permutation registry is hard coded.** `k_ModuleSpaces` in `PermutationSpace.cpp` names
   `OceanFft`, and `k_OceanFftPolicy` holds project data. A data-driven registry is future work.
4. **`ShaderManifest.cpp` uses magic_enum for one `ToString`.** The runtime target should carry no
   third-party dependency. The repository hand-writes the same switch elsewhere.
5. **Stage 3 and stage 4 are fused.** `SlangCompiler.cpp` both talks to Slang and resolves attributes,
   size expressions, and the WGSL cross-check. The separation pass is the next planned work, and it is
   what lets a target other than WGSL exist.
6. **A stage gets bindings that it does not touch.** `GetBindings(entry_point, variant)` gives the
   whole program-scope list, and `BindingInfo` holds no usage mask. `MainVS` therefore gets a layout
   that holds `BaseColorTexture` and `BaseColorSampler`, which the vertex stage never reads. A caller
   that builds a bind group layout from this over-declares, and WebGPU then demands resources that the
   stage never touches. The fix moves `EntryPointUsageMask` and changes the interned layout key, so it
   is a separate pass.
7. **`ReportUnreferencedBindings` has three faults.** Commit `1bba390` added it, before this change
   set. The call sits behind `options.ReportReflection`, so `--quiet` turns the check off. It costs
   O(bindings squared x entry points) for each variant, because the inner search re-finds an index
   that the outer loop already holds. It also repeats one answer for each variant that shares a layout
   tuple.

---

## 6. Useful review questions

Ask these. They test the parts that matter.

- Does any emitter read something other than `CookedLibrary`?
- Does any change let the hash decide equality without a byte comparison?
- Does `--no-dedupe` still give the same bytes as a deduped cook?
- Does any new code path skip the round trip check?
- Does canonicalization reach the linker? It must not. Only `Active` may.
- Does any Slang type or WGSL string reach `ShaderDataSchema.hpp`?

---

## 7. Evidence from the last full run

| Measure | Value |
|---|---|
| Nominal permutations | 56 |
| Permutations after canonicalization | 35 |
| Artifacts | 105 |
| Unique sources | 77 |
| Unique layouts | 21 |
| Hash collisions | 0 |
| Byte comparisons forced by a hash hit | 112 |
| A/B pairs identical, baked against manifest | 168 |
| WGSL address space declarations checked | 231 |
| Manifest size against generated C++ | 657 KiB against 708 KiB |

`IfftPermuteCS` is inert on both wave axes and collapses 35 sources to 7.

Layouts do not collapse to one. 21 is 3 entry points by 7 sizes, because `[vx_element_count]` makes a
layout depend on the size. A consumer must hold a layout for each entry point and size, not one for
each module.
