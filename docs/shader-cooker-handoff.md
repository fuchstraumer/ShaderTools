# Shader cooker handoff

For a future agent instance. This document tells you the state of the work, the rules you must not
break, and the next task in order.

Read `docs/cooker-rendergraph-plan.md` first. It is the design document. This file is the delta.

---

## 1. State

The agreed scope was Tier A, Tier B, and C1 to C4. All of it is complete. Tier D and item C5 were cut.

| Tier | Result |
|---|---|
| A | Non-positional permutation keys, size expressions, attributes, full reflection payload, C++ emitter, provider seam |
| B | Content interner, provenance, dedup report, axis influence matrix, determinism check |
| C1 | WGSL address space cross-check |
| C2 | Binary manifest and its reader |
| C3 | Raster reflection and the `half4` probe |
| C4 | Uniform block member offsets |

Measured numbers on `OceanFft.slang`, for regression comparison:

- 56 nominal permutations, 35 after canonicalization
- 105 artifacts, 77 unique sources, 21 unique layouts
- 0 hash collisions, 112 byte comparisons
- 168 A/B pairs identical between the baked library and the manifest

---

## 2. Rules you must not break

Start each change with these. A change that breaks one is wrong, even if the cook exits 0.

1. **The hash never decides equality.** It selects a bucket. A byte comparison decides. Count each
   collision and report it. Never resolve a collision in silence.
2. **Keep stage 5 apart from stage 6.** "These are the same" and "so keep one" are different jobs.
3. **Keep the identity path.** `--no-dedupe` must always give correct output. It is the A/B oracle.
4. **Run the round trip check on every cook.** Do not put it behind a flag.
5. **The emitted artifact decides group and binding numbers. Reflection decides sizes and types.**
   This asymmetry is the whole reason the cross-check finds errors.
6. **Every generated artifact reads one frozen model.** An emitter must not reach past `CookedLibrary`.
7. **An axis name must match the Slang `extern const static` name exactly.** A mismatch links a symbol
   that nobody references. The shader keeps its default value and nothing fails.

---

## 3. Next task: the stage separation pass

This is the key work. Do it first.

### Why it matters

The cooker targets WGSL only. `target.format = SLANG_WGSL` is one line, but the toolchain is not
ready for a second target, because two jobs are fused.

`SlangCompiler.cpp` does two different things today:

- It talks to Slang and gets bytes and raw reflection. This is stage 3, Compile.
- It evaluates size expressions, reads `[vx_*]` attributes, and cross-checks the emitted WGSL. This is
  stage 4, Resolve.

The cross-check is target specific by construction. `WgslBindingScanner` reads WGSL text. A SPIR-V
target needs SPIRV-Reflect. A DXIL target needs DXC reflection. While stage 3 and stage 4 stay fused,
each new target duplicates the compile path too.

**Separate stage 3 from stage 4, and the target language becomes a parameter.** Stages 5 to 8 stay
shared, because they already read a model that names no Slang type and no WGSL text.

This also makes stage 4 testable without Slang. `SizeExpressionTests` proves the value of that: it is
a pure function, it has 28 checks, and it builds under Emscripten.

### What exists already

Four of the eight stage boundaries are real types. Do not rebuild them.

| Stage | Type | Source |
|---|---|---|
| 2 Enumerate | `VariantSet`, `VariantDescriptor` | `PermutationSpace.hpp` |
| 6 Intern | `ContentInterner` with `Disable()` | `ContentInterner.hpp` |
| 7 Key | `CookedLibrary`, `CookedModule` | `CookedLibrary.hpp` |
| 8 Emit | three emitters over the frozen model | `ShaderLibraryEmitter`, `ShaderManifestEmitter`, `DedupeReport` |

### What to build

1. Add a `RawLibrary` type. It holds compiled bytes and raw reflection. It names no `[vx_*]` attribute
   and no size expression.
2. Move all Slang contact into stage 3. Stage 3 returns `RawLibrary`.
3. Move size expressions, attribute reads, and the cross-check into stage 4. Stage 4 returns
   `ResolvedLibrary`.
4. Make the cross-check an interface. `WgslBindingScanner` becomes one implementation.
5. Leave stage 5 empty. The report says `normalization passes active: (none)`. This is correct. A
   whitespace pass without a stage boundary hides the difference between a true collapse and an effect
   of the stripping.

---

## 4. Other agreed work

Do these after the stage split, or before it if the user asks.

### A stage gets bindings that it does not touch

**Correctness. Fix this before you show the tool to anyone.**

`GetBindings(entry_point, variant)` gives the full program-scope binding list. The list includes each
binding that the entry point never reads. `BindingInfo` holds no usage mask, so a caller cannot tell
the two apart.

The raster module proves it. `MainVS` gets a layout with `BaseColorTexture` and `BaseColorSampler`.
The vertex stage reads neither.

```
MainVS   Scene usedBy=0x1   BaseColorTexture usedBy=0x0   BaseColorSampler usedBy=0x0
MainFS   Scene usedBy=0x2   BaseColorTexture usedBy=0x2   BaseColorSampler usedBy=0x2
```

A caller that builds a bind group layout from this over-declares. WebGPU then demands resources that
the stage never touches. Vulkan and DX12 have the same rule.

Decide where `EntryPointUsageMask` lives. There are two choices:

- Filter before the intern. An entry point layout then holds only the bindings that it reads.
- Keep the full list and put the mask in `BindingInfo`. The caller filters.

**Warning: this changes the interned layouts.** The mask is part of the layout key today, because
`HashLayoutPayload` hashes it and the defaulted `operator==` compares it. One raster variant with two
entry points gives two layouts for this reason alone. Expect the OceanFft layout count to fall from 21
to 7 when the mask leaves the key. Re-measure each number in section 1 and correct this document.

### The unreferenced binding check

`ReportUnreferencedBindings` in `CookerDriver.cpp` has three faults. Commit `1bba390` added it.

1. **It does not always run.** The call sits behind `options.ReportReflection`, and `--quiet` clears
   that flag. A quiet cook makes no check. Decide if this is a diagnostic or a check. If it is a
   check, run it always, and let the flag control the print only.
2. **It costs too much.** The cost is O(bindings squared x entry points) for each variant. Each entry
   point holds the same binding set in the same order. The inner search finds an index that the outer
   loop already has. Use the index and the cost falls to O(bindings x entry points).
3. **It repeats itself.** It runs for each variant, but the answer changes only with the layout tuple.
   35 variants give at most 21 answers, and the console gets 35 copies of each line. Move the check
   after `FreezeModuleTables`. Skip a variant whose `LayoutIndices` tuple you saw before.

**Note**: Points 2 and 3 were fixed in commit `ecf6dde`

Rename the data while you are here. `variant.EntryPoints.front().Reflection.Bindings` hides why the
first element is special: every entry point holds the same set, and only the mask differs. Put the set
in `CompiledVariant::GlobalBindings`. Put a parallel `UsageMask` in each `CompiledEntryPoint`.

The rename touches `SlangCompiler.cpp` and four functions in `CookerDriver.cpp`
(`ReportEntryPointReflection`, `SelectBindingsUsedByEntryPoint`, `ReportUnreferencedBindings`,
`ValidateVariantReflection`), plus `AppendVariantToModule` in `CookedLibrary.cpp`. It touches no
emitter, because each emitter reads `CookedLibrary` and not `CompiledVariant`.

Do the rename alone first. Rebuild the per-entry-point view at the intern call site, so the layout
count does not move. Then do the mask decision above as a separate pass.

### Library-wide manifest

The manifest holds one module today. The user chose a manifest that holds many modules. It packs the
data better, and modules will share data later.

- Change `EmitShaderManifest` to take `CookedLibrary`, not `CookedModule`.
- Add a module table. Add a dense list of entry point indices for each module.
- Add `GetModuleEntryPoints(name)` to `ShaderManifestView`.
- This is a stage 8 change only. Nothing before stage 8 must change.

### Entry point identifier width

`EntryPointId` is `uint16_t` today. `Invalid` is 0, so the first entry point is 1, and `FindSlot`
subtracts one. A `todo-ship` in `ShaderLibraryTypes.hpp` records this.

Change it to `int32_t` with `Invalid = -1` and make the identifiers count from zero. Then the
identifier is the table index, and the adjustment goes away. Touch these files:

- `include/shader/ShaderLibraryTypes.hpp` — three virtual functions and the comment above them
- `include/shader/ShaderManifest.hpp` — `FindSlot` and the three overrides
- `src/shader/ShaderManifest.cpp` — the zero guard and the `- 1u` in `FindSlot`
- `src/ShaderLibraryEmitter.cpp` — the emitted enum, its width, the first ordinal, and each override
- `src/ShaderManifestEmitter.cpp` — the `i + 1u` in `VerifyManifestRoundTrip`

The file format does not change. Entry point identifiers are never stored. Their order in the slot
table gives them.

### Name lookup

The provider takes an integer and gives no way to get one. Add `ResolveEntryPoint(name)`. Resolve once
at setup and hold the integer. The hot path stays an array index. The cooker knows every name at cook
time, so a sorted name table and a binary search are enough. Do not add `unordered_map`.

### Repository extraction

The user plans to make this the new ShaderTools. One coupling blocks it:
`ShaderLibraryTypes.hpp` includes `resource/ResourceFlags.hpp` for `TextureFormat`. Invert it. The
library must own its own format enum. The engine maps it at the boundary.

Two other gaps against a multi-backend target: `BindingKind` is WebGPU shaped, and the schema has no
push constants and no specialization constants. Vulkan and DX12 need both.

---

## 5. Design directions

Nobody scheduled this work. Each part records a decision and gives the words to search.

### Give diagnostics a structured sink

The cooker formats each error as text where the error happens. A live-edit session cannot use that.

Do this instead:

- Parse the Slang blob into a record at the boundary. Give the record a severity, a file, a line, a
  column, a code, and a message.
- Report each record to an abstract sink. Compilation must never format a string.
- Write one sink for stderr. Write a second sink that fills a fixed ring buffer for an ImGui console.

Take the record shape from the Language Server Protocol `Diagnostic` type. Editors already read it.

Slang solves the same problem in `source/compiler-core/slang-diagnostic-sink.h`. Clang solves it with
`DiagnosticConsumer`, and writes a byte stream with `-fdiagnostics-format=json`.

The cooker runs out of process today. Keep it there. A shader that kills the compiler must not kill
the editor.

Terms in space: diagnostic sink, diagnostic consumer, structured diagnostics, language server protocol
diagnostic.

### Give each target a capability profile

`ShaderLibraryTypes` declares the union of every concept. One target supports a subset. State which.

Give each target a profile. Each value of the vocabulary gets one of three outcomes:

| Outcome | Meaning |
|---|---|
| Supported | maps to a native concept |
| Lowered | maps to something else, at a cost that the profile states |
| Unsupported | a hard error that names the construct and the target |

The third row makes this a tool instead of a guess. WGSL has no push constant. Vulkan has one. The
WGSL profile must say so and must name the declaration.

Delete `default:` from each switch over an enum that this repository owns. Make `-Wswitch` an error.
The compiler then names each new case. Keep `default:` for a Slang enum, because a Slang release must
not break the build. **Report the unmapped value through the sink.** A silent `Invalid` return repeats
the failure shape of an axis name typo.

Add `static_assert(magic_enum::enum_count<T>() == N)` beside each mapping table.

Slang has a capability system for this exact problem. Read `slang-capability.h` and the `[require()]`
attribute.

Search: capability system, target profile, feature matrix, progressive lowering, exhaustive switch.

### Move the permutation model out of C++

`PermutationAxis` holds one parent and one required value. That gives a tree. Real shaders need more.
One axis can need two conditions at once. `USE_LIGHTMAP` needs `ENABLE_PARTICLE_LIGHTING` **and**
`LIGHTING_TECHNIQUE == COMPLEX`. The current type cannot state this.

**This is a solved problem, and it has a name: a feature model.** Read about software product lines.
Read Kconfig, which the Linux kernel uses. A feature model is a tree of options plus cross-tree
constraints, such as `requires` and `excludes`.

Three parts follow.

1. **Constraints.** Replace `Parent` and `RequiredParentValue` with a boolean expression over the
   other axes. Extend `SizeExpression` to evaluate it. That evaluator names no Slang type, and it has
   tests already.
2. **Enumeration.** A depth-first walk with constraint propagation gives each valid assignment. The
   general form of this question is SAT. Counting the answers is #SAT. Shader spaces stay small, so a
   plain walk is enough. Do not add a solver.
3. **Index.** Mixed-radix over the full product leaves too many holes once constraints grow. Use
   ranking instead. Count the valid completions of each prefix. Bake the counts as a constexpr table.
   Sum the skipped counts. The index stays dense, stable, and constexpr.

Keep all three of the current mechanisms. The canonical form is the cheap guess before the compile.
Interning is the exact measurement after it. The influence matrix reports when the guess is too
coarse. Few systems hold all three. Do not drop one.

**Declare each axis in the shader, not in a data file.** Use an attribute, as `[vx_element_count]`
does. Three reasons:

- The name cannot drift, because the axis is the declaration.
- `VerifyAxisNamesAreDeclared` becomes unnecessary, and its failure goes away.
- The language server sees each axis.

This costs one extra compile. The cooker must compile the module once with defaults, read the
attributes, and then enumerate. Accept that cost.

**Keep policy in a data file.** A variant budget, an expected influence, and the subset to cook belong
to a project, not to a shader. `ModulePolicy` and `PermutationSpace` already split this way. Use JSON
or TOML. Avoid YAML, because the parsers are large.

Cook only the subset that a project uses. Unity splits `multi_compile` from `shader_feature` for this
reason. Unreal uses `ShouldCompilePermutation`. Both state one rule: a declared axis is not always a
cooked axis.

Search: feature model, software product line, Kconfig, cross-tree constraint, ranking and unranking,
model counting, shader permutation reduction.

---

## 6. Sharp edges

- **The subtree does not build alone.** `git subtree split -P tools/shader_cooker` keeps only the
  cooker. It does not keep `include/shader/*`, `src/shader/*`, `assets/shaders/*`, or the tests.
  Section 3 of the change summary lists each missing file.
- **A failed CMake configure is expensive.** It forces a full Slang rebuild of about 10 minutes.
  Confirm the environment before you configure.
- **Wrap the build with vcvars64.** Bare `cmake` fails outside VS Code. Write a `.bat` file. Do not
  chain `cmd /c "call ... && cmake"` through Git Bash, because the quotes do not survive.
- **The cooker logs to stderr.** Never use `2>&1` on a native executable in PowerShell 5.1. It reports
  a false non-zero exit code. Pipe to `Out-Null` and read `$LASTEXITCODE`.
- **The permutation registry is hard coded.** `k_ModuleSpaces` in `PermutationSpace.cpp` names
  `OceanFft`. An unknown module gets an empty space and cooks one variant.
- **The help text is stale.** `--no-dedupe` and `--verify-deterministic` both work. `k_UsageText` in
  `CookerOptions.cpp` lists neither.
- **String literals split at 8192 bytes.** MSVC error C2026 caps a literal at 16380 bytes.
- **The manifest byte span must start on an 8-byte boundary.** `Open()` returns `Misaligned` if it
  does not.

---

## 7. How to verify

Run each of these. All must pass.

1. Cook `assets/shaders/compute/OceanFft.slang`. Exit 0 means every variant compiled, the reflection
   matched the emitted WGSL, and both round trips passed.
2. Cook `assets/shaders/render/VertexFormats.slang`. This is the raster path and the `half4` probe.
3. Cook with `--no-dedupe`. Compare each `(entry point, variant)` against the deduped cook. The bytes
   must match.
4. Cook with `--verify-deterministic`. It cooks twice into memory and compares the artifacts.
5. Read the dedup report. Confirm `IfftPermuteCS` shows both wave axes inert.
6. Run `ctest`. `MathTests` and `SizeExpressionTests` must pass.
7. Compare the emitted WGSL against `include/generated/OceanShadersReflected.hpp`. The 77 sources must
   match the baseline. The old emitter added one leading newline to each string, so remove that first.
