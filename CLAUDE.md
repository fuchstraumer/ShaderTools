# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

The style rules live in `.github/copilot-instructions.md`. Read that file first, and obey it.
This file adds code context only. It does not repeat the style rules.

Write your text in ASD-STE100 (Simplified Technical English), as the style file says.

## Build and test

The project uses CMake presets with the Ninja Multi-Config generator. Each preset builds into
`build/<presetName>`. Slang and xxHash are git submodules in `third_party/`.

```bash
git submodule update --init --recursive
```

```bash
cmake --preset ninja-msvc
```

```bash
cmake --build build/ninja-msvc --config Debug
```

Presets: `ninja-msvc` and `ninja-clang-cl`. Configurations: `Debug` and `RelWithDebInfo`.
Clang-CL with the MSVC frontend variant is a hard configure error.

No build turns on a sanitizer. AddressSanitizer on Windows does not support the debug CRT. ASan
intercepts `malloc` and `free`, `ucrtbased.dll` allocates through `_malloc_dbg`, and ASan then reports
the teardown free as a bad free. The test aborts before `main`, and no frame of the stack belongs to
this repository. To turn the sanitizers on, first move every target in the process to the release CRT,
slang included.

Check the exit code of the build itself. A pipe into `tail` gives you the exit code of `tail`, and a
failed build then looks like a success.

Run every test:

```bash
ctest --test-dir build/ninja-msvc -C Debug --output-on-failure
```

Run one test:

```bash
ctest --test-dir build/ninja-msvc -C Debug -R SizeExpressionTest --output-on-failure
```

Each test target is also a standalone executable. Run it directly to debug it:
`build/ninja-msvc/tests/Debug/CookTest.exe`.

There is no test framework. `tests/TestHarness.hpp` gives a counter, `Check(condition, description)`,
and a nonzero exit code.

Six test targets exist. Five are unit tests, and each one proves a claim the repository makes. None of
them needs Slang, a compiler, or an asset, and all five together run in under one second.

| Target | Proves |
|---|---|
| `SizeExpressionTest` | The size expression grammar, and every rejection it must make. |
| `ContentInternerTest` | A hash never decides equality. It supplies a hash that returns one constant, so only the byte comparison can separate the payloads. |
| `PermutationIndexTest` | A variant index is unique, dense, and stable, and a partial assignment resolves to one variant. |
| `ShaderManifestRejectTest` | The manifest reader rejects a short, misaligned, or damaged file, and opens a real one. |
| `WgslBindingScannerTest` | The cross-check reads the emitted WGSL correctly, and fails on a real mismatch. |

An error check prints a diagnostic to `stderr` on purpose. Read the last line for the result.

`CookTest` is the sixth, and it is different. It is the cooker driver, not an assertion suite.
`tests/CMakeLists.txt` gives it a command line through `TEST_ARGS`, so `ctest` runs a real cook of
`OceanFft.slang` with `--verify-deterministic`. It takes about 18 seconds, and it is the only
end-to-end coverage. Exit code 0 there is a real statement: every variant compiled, every reflection
agreed with the emitted WGSL, both round trips read back the same bytes, and two cooks agreed byte for
byte.

Add a test with `add_lodestone_unit_test(<Name> <Name>.cpp)`. Add `TEST_ARGS <args>` after the sources
when the test needs a command line.

## Running the cooker

The cooker has no dedicated CLI target today. `lodestone` is a static library, and `CookTest.cpp`
holds the only `main` that drives a cook. Callers build `CookerOptions`, build an `OutputSink`, and
call `RunCook`. `ParseCommandLine` in `src/CookerOptions.cpp` parses the flags, and `GetUsageText`
prints them: `--output/-o`, `--cache-dir`, `--O0` to `--O3`, `--no-validate`, `--quiet`,
`--single-threaded`, `--no-dedupe`, `--verify-deterministic`.

`lodestone` must stay `STATIC`. No header marks a symbol `dllexport`, so a DLL build of this target
exports nothing and every consumer fails to link. A `SHARED` build is for instrumented performance
analysis only, and it needs `WINDOWS_EXPORT_ALL_SYMBOLS` to link at all.

`tools/manifest_dump` is a real executable. It reads one `.ldshaders` manifest and writes JSON. It
links only `lodestone::client_internal`, never the cooker or Slang. That link line is the proof that
the client half stays free of the compiler.

Test shaders live in `tests/assets/`. `tests/assets/compute/Ocean/OceanFft.slang` is the reference
module, because it is the only module with a registered permutation space.

## The two problem domains

The repository serves two domains, and the whole design follows from the split.

- The authoring domain wants flexible shaders, readable parameters, and fast edits.
- The runtime domain wants small data, fast loads, no main-thread stalls, and no undefined behavior.

The cooker is the compiler between them. `client/include/ShaderLibraryTypes.hpp` is the contract.
A renderer links against that header, and against nothing else in this repository. A new shader
therefore does not force the renderer to rebuild.

Do not put a Slang type, a WebGPU type, or a `std::filesystem` type into the client headers or into
`ShaderDataSchema.hpp`. A compiler type that leaks into the schema becomes a dependency the engine
must carry forever.

## Data flow, one cook

`RunCook` in `src/CookerDriver.cpp` is the whole loop. Read that file first. It calls each stage in
order, for each module path.

1. **Space.** `FindPermutationSpaceForModule(name)` finds the module's axes.
   `VerifyAxisNamesAreDeclared` checks each axis name against the `extern const static` declarations
   in the Slang source texts. A module with no registered space gets an empty space and one variant.
2. **Enumerate.** `EnumerateVariants` expands the space into a `VariantSet` of `VariantDescriptor`
   values. Each descriptor holds `Active` and `Canonical` (see below) and a dense index.
3. **Compile.** `SlangCompiler::CompileVariant` links and generates. It builds one synthetic Slang
   module for each active axis value, composites those with the base module, links, then asks Slang
   for WGSL for each entry point. Entry point codegen runs on `std::async` unless
   `MultithreadEntryPointCodegen` is false. Output is `CompiledVariant`.
4. **Resolve.** The same call extracts reflection, reads the `[vx_*]` attributes, and evaluates each
   size expression against the axis values. Output is the reflection part of `CompiledVariant`.
   Stage 3 and stage 4 are fused inside `SlangCompiler.cpp` today. `docs/shader-cooker-handoff.md`
   asks for the split, because the split is what makes a second target language possible.
5. **Cross-check.** `ValidateVariantReflection` scans `@group`/`@binding` back out of the emitted
   WGSL with `WgslBindingScanner`, then compares that against the bindings the entry point uses.
   A mismatch increments a counter, and a nonzero counter fails the cook with
   `CookError::ReflectionMismatch`.
6. **Intern.** `AppendVariantToModule` gives each source, each layout, and each raster state to a
   `ContentInterner`. `FreezeModuleTables` copies the unique entries into the `CookedModule`.
7. **Key.** Each `LibraryVariant` holds only indices into those tables, plus its dense index.
8. **Verify.** `VerifyLibraryRoundTrip` replays every variant through the finished tables and
   compares the result against the text the compiler produced. It runs on every cook.
   `EnforceModulePolicy` then checks the measured axis influence against `ModulePolicy`.
9. **Emit.** `EmitLibraryArtifacts` writes through the `OutputSink`:
   - the C++ header, `sink.PrimaryName()`, from `EmitShaderLibraryHeader`
   - one C++ source for each module, `<headerStem>_<Module>.cpp`
   - one binary manifest for each module, `<Module>.ldshaders`, plus `VerifyManifestRoundTrip`
   - `ShaderLibrary.dedupe.txt`, from `GenerateDedupeReport`

`--verify-deterministic` runs steps 1 to 9 twice into two `MemoryOutputSink` objects, compares every
artifact byte for byte, then writes the first result to the real sink. A difference means an
unordered container reached the output.

## Major types

| Type | Header | Role |
|---|---|---|
| `CookerOptions` | `CookerOptions.hpp` | Every knob one cook has. `ParseCommandLine` fills it. |
| `OutputSink` | `OutputSink.hpp` | Where artifacts go. `FileOutputSink` and `MemoryOutputSink`. The seam a live cooker will use. |
| `PermutationAxis`, `PermutationSpace` | `PermutationSpace.hpp` | One axis of variation, and the list of axes for a module. |
| `VariantDescriptor` | `PermutationSpace.hpp` | One variant identity. `Active` and `Canonical`. |
| `ModulePolicy` | `PermutationSpace.hpp` | A variant budget, plus the axis influence the author expects. |
| `CompiledVariant`, `CompiledEntryPoint` | `ShaderDataSchema.hpp` | Compiler output: WGSL text plus reflection. Owns its strings. |
| `ReflectedBinding` | `ShaderDataSchema.hpp` | What the shader states about one resource. The CPU side never writes any of it. |
| `ContentInterner<T>` | `ContentInterner.hpp` | Collapses equal payloads, keeps provenance, counts collisions. |
| `CookedModule`, `CookedLibrary` | `CookedLibrary.hpp` | The frozen model. Every emitter reads this and nothing earlier. |
| `ShaderManifestView` | `client/include/ShaderManifest.hpp` | Read-only spans over the manifest bytes. Allocates nothing to open. |
| `ShaderSourceProvider` | `client/include/ShaderLibraryTypes.hpp` | Where a renderer gets source, bindings, and workgroup size. `Generation()` is the hot-reload hook. |

`ContentHashValue` is FNV-1a 64 today. The hash name reaches the output, so a new hash needs a new
name.

## Rules the code depends on

Break one of these and the cook can exit 0 with wrong content.

1. **A hash never decides equality.** It picks a bucket. A byte comparison decides. The interner
   counts each collision and reports it. It never resolves one in silence.
2. **`--no-dedupe` must stay correct.** It is the A/B control arm. Both modes must emit valid output.
3. **The round trip checks are never optional.** `VerifyLibraryRoundTrip` and
   `VerifyManifestRoundTrip` run on every cook.
4. **The emitted artifact decides group and binding numbers. Reflection decides sizes and types.**
   That asymmetry is the only reason the cross-check finds real errors.
5. **Every emitter reads one frozen model.** An emitter must not reach past `CookedLibrary` into
   `CompiledVariant` or into Slang.
6. **An axis name must match the Slang `extern const static` name exactly.** A mismatch links a
   symbol that nobody references, leaves the shader on its default, and fails nowhere.
   `VerifyAxisNamesAreDeclared` exists for this reason.

### `Active` and `Canonical`

`Active` holds only the axes this variant really uses. It drives the Slang linker, so it decides the
emitted text. `Canonical` holds every axis in declaration order, with the first value filled in for a
disabled axis. It drives the dense mixed-radix index only. Canonicalization therefore can never
change shader output, and a caller can find a variant with a partial set of values.

`SpaceSize` counts the dense index range with the holes included. A disabled dependent axis leaves
gaps, and the design accepts them.

### Size expressions

A size travels as a string, and this is not a style choice. Slang folds an attribute integer argument
at compile time, but the permutation constants are `extern const static` and fold at link time.
`[vx_element_count(IFFT_SIZE * 4)]` therefore fails to compile. A string argument reaches reflection
untouched, and `EvaluateSizeExpression` does the arithmetic once for each variant.

The attribute declarations are in `tests/assets/LodestoneAttributes.slang`: `vx_element_count`,
`vx_extent_2d`, `vx_extent_3d`. Slang has no optional attribute parameters, so each arity needs its
own name. The README shows `lodestone_element_count`; the code says `vx_element_count`.

## Where to register a module

`FindPermutationSpaceForModule` and `FindPolicyForModule` read a static table, `k_ModuleSpaces`, at
the top of `src/PermutationSpace.cpp`. Add an axis, a space, a policy, and a table row there. Only
`OceanFft` has an entry today. This table is compiled in, and moving it to data is future work.

## Two output forms, one model

The generated C++ and the binary manifest carry the same tables from the same `CookedModule`.

- The C++ form compiles into the program. The header holds identity and lookup only, never shader
  text, so naming a shader does not rebuild when the text changes.
- The manifest form arrives as bytes. Every cross-reference is a `uint32` index, never a pointer, so
  the reader is a set of spans and it relocates nothing. Sections start on 8-byte boundaries. Record
  sizes are pinned with `static_assert`, because they are part of the file format.

`todo.md` states the author's plan to delete the C++ emitter once the manifest path is complete. Do
not start that removal without a request.

## The velox rename

This repository was extracted from an engine named `velox`. The C++ rename to `lodestone` is
complete. No `velox` name remains in `src/`, `include/`, `client/`, `tests/`, or `tools/`.

The shader side keeps the old prefix on purpose. `tests/assets/LodestoneAttributes.slang` declares
`module VeloxAttributes` and the `vx_element_count`, `vx_extent_2d`, and `vx_extent_3d` attributes.
Those names are part of the shader-side contract, and `src/SlangCompiler.cpp` reads them by string. A
rename there touches every test shader, so treat it as its own task.

## Documents

- `docs/cooker-rendergraph-plan.md` — the design document. It comes from the engine repository, so it
  frames the rendergraph as the consumer. The real goal is any rendering backend.
- `docs/shader-cooker-handoff.md` — state, invariants, and the agreed next tasks. The stage 3 and
  stage 4 split is the first one.
- `docs/shader-cooker-change-summary.md` — the eight-stage pipeline, and a commit map.
- `README.md` — the author's intent. It mixes shipped features with planned ones. Check the code
  before you trust a feature claim there.
