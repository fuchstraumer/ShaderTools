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

## 5. Sharp edges

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

## 6. How to verify

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
