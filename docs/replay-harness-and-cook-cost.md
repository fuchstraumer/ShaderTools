# The replay harness, and the cost of the cook

Written 2026-08-18. This document does two jobs.

1. It specifies a harness that removes Slang from an instrumented cook, so that a profiler can see
   this repository's own code.
2. It lists the costs the survey found in that code, in the order of their size.

**Scope.** Every item is in `src/` or `include/`. Another agent owns those directories today. This
document is a specification and a defect list. It is not a change.

**Terms.** A *cook* is one run of `RunCook`. A *variant* is one point in the permutation space. An
*entry point* is one shader function. A *capture* is the file this harness writes. A *replay* is a
cook that reads a capture in place of Slang. *Live* describes a cook that calls Slang.

---

## 1. Why the harness must exist

`getEntryPointCode` is the largest cost in a live cook. That fact hides every other cost. A sampling
profiler puts almost every sample inside Slang. An instrumented build makes the same statement more
slowly.

The volume tiled forward module improves the utilization but not the visibility. It declares 19 entry
points against the 3 that `OceanFft` declares. `GenerateEntryPointCode` starts one `std::async` task
for each entry point, so 19 tasks fill more cores than 3 tasks fill. The wall time for each variant
falls. The share of the wall time that Slang holds does not fall.

The goal is different. This repository must be an almost-zero-cost abstraction over a large feature
set. To make that claim, the cost of this repository's own code must be a number that somebody reads.
Today that number cannot be measured, because it is under the Slang number.

---

## 2. Where the seam goes

**Put the seam at `RawVariant`, and not at `getEntryPointCode`.**

Three reasons.

1. **`RawLibrary.hpp` already names no Slang type.** `RawVariant` is a complete description of stage 3
   output. So a replay needs no new model. It needs a serializer for a model that exists.
2. **The link is also a Slang cost.** A replay that supplies the target text alone must still call
   `link` to get reflection, because `ExtractRawBindings` reads a `ProgramLayout`. That replay saves
   the codegen and keeps the link. A replay at `RawVariant` removes both.
3. **The interface is already the compiler's public surface.** `PrepareModuleCompiler` and
   `CookModule` call five members of `SlangCompiler`: `GetModuleName`, `GetEntryPointNames`,
   `GetModuleSourceTexts`, `GetExternConstantDefaults`, and `CompileVariantRaw`. Nothing else in the
   driver touches the compiler. So the extraction is small.

The shape:

```cpp
/** Stage 3, behind an interface. A live source calls Slang. A replay source reads a capture. */
class RawVariantSource
{
public:
    virtual ~RawVariantSource() = default;
    virtual CookResult<RawVariant> Produce(const VariantDescriptor& descriptor) = 0;
    virtual std::string_view ModuleName() const noexcept = 0;
    virtual std::span<const std::string> EntryPointNames() const noexcept = 0;
    virtual std::span<const std::string> ModuleSourceTexts() const noexcept = 0;
    virtual std::span<const ExternConstantDefault> ExternDefaults() const noexcept = 0;
};
```

`SlangVariantSource` holds a `SlangCompiler`. `ReplayVariantSource` holds one byte buffer.

This follows the rule the repository already applies to a target validator. Phase D step D7 gives the
validator an interface for the same reason: the driver must not know which implementation answers.

---

## 3. The capture file

### What it holds

One file for each module, named `<Module>.rawcapture`. It holds every `RawVariant` of one cook.

Give it the discipline of the manifest, because the reason is the same. Every cross-reference is a
`uint32` index. No field is a pointer. Each section starts on an 8-byte boundary. Each record size is
pinned with a `static_assert`.

Sections, in order:

| Section | Holds |
|---|---|
| Header | magic, version, input signature, entry point count, variant count, section table |
| Text table | the unique target texts, end to end |
| Text index | one offset and one length for each unique text |
| String table | every name and description, end to end |
| Binding records | one fixed record for each `RawBinding` of each variant |
| Attribute records | one record for each `RawSizeAttribute` |
| Entry point records | one record for each `RawEntryPoint`, with an index into the text index |
| Variant records | one record for each variant, with ranges into the three sections above |

### Dedupe the text inside the capture

This is not optional at the tier sizes the explosion plan asks for.

Measured: the 19 volume tiled forward entry points give 4741, 16917, 8957, and 3019 bytes for four
samples of the compute set. Call the mean 8 KiB. One variant therefore holds about 150 KiB of target
text.

| Tier | Variants | Text, no dedupe | Text, deduped, estimated |
|---|---|---|---|
| B | 480 | 72 MiB | under 5 MiB |
| C | 2 880 | 432 MiB | under 10 MiB |
| D | 414 720 | 61 GiB | not applicable, see below |

The dedupe estimate follows from the axis influence. Most entry points are inert to most axes, so
most entry points give one text for many variants. `OceanFft` already shows the effect: 105 entry
point instances collapse onto 77 unique texts, and that module has no presence axis.

Use `ContentInterner` for this. It is the same collapse the cook already performs, and it counts each
collision.

Tier D never reaches a capture. The variant budget must stop it at stage 2. See section 8.

### Refuse a stale capture

A profile against stale data is worse than no profile, because it looks correct.

Put an **input signature** in the header. It is one content hash over four things: every string in
`ModuleSourceTexts`, the target name, the optimization level, and the permutation space. If any of
those changed, `ReplayVariantSource` must fail the cook and name the difference. It must not warn and
continue.

`ModuleSourceTexts` already holds every file the module pulled in, transitively. So the signature
needs no new collection pass.

---

## 4. How to prove the replay is faithful

The repository does not assert that dedupe is correct. It replays and compares. Apply the same rule
here.

**The check:** cook once live with `--capture-raw`. Cook again with `--replay-raw`. Compare every
artifact byte for byte.

`RunCookTwiceAndCompare` already holds that comparator. It cooks twice into two `MemoryOutputSink`
objects and compares the content and every artifact. A replay check reuses it, with one arm live and
one arm replayed.

If the two cooks emit identical bytes, the replay is faithful. This is a validator, and not an
assertion. It compares two answers that were derived independently: one from Slang, one from a file.

Add this as a test target. It is cheap, because `OceanFft` cooks in about 18 seconds and the replay
arm costs almost nothing.

---

## 5. What the harness does not measure

State this beside every number the harness produces.

1. **It does not measure Slang.** That is the purpose, and it is also the limit. A replay profile
   says nothing about the live wall time.
2. **It does not remove allocation.** `Produce` builds a `RawVariant` that owns its strings, because
   the live path builds the same thing from Slang blobs. The two paths therefore allocate the same
   way, which is what makes the profile representative. The saving is the link and the codegen, and
   nothing else.
3. **It does not model a live cooker.** A live edit cooks one variant. A replay cooks the whole
   space. The two workloads have different shapes.
4. **The read at start is one cost, and it is not free.** Measure it separately and report it. Do not
   fold it into the per-variant numbers.

---

## 6. Costs found in the survey

The list is in the order of size. The tier C column uses 2880 variants, 19 entry points, and 10 axes.

### 6a. `ComputeAxisInfluence` is quadratic in the variant count, and it runs twice

`InfluenceOfAxis` at `src/DedupeReport.cpp:85` compares every pair of variants. `ComputeAxisInfluence`
at `:246` calls it once for each entry point and each axis. So the cost is `E × A × V² / 2` calls to
`DiffersOnlyInAxis`, and each call walks the assignment.

| Module | Pair tests |
|---|---|
| `OceanFft`, 3 entry points, 3 axes, 35 variants | about 5 500 |
| Tier C, 19 entry points, 10 axes, 2880 variants | about 787 000 000 |
| Tier D, 19 entry points, 18 axes, 414720 variants | about 2.9 × 10¹³ |

It runs **twice for each cook**. `FinalizeModule` at `src/CookerDriver.cpp:575` computes it for
`EnforceModulePolicy`. `GenerateDedupeReport` at `src/DedupeReport.cpp:471` computes it again for the
report. The first result is discarded. With `--verify-deterministic` the total is four times.

**This is the largest cost in this repository's own code, and it is not Slang.** At tier C it is tens
of seconds. At tier D it does not complete.

**The correct algorithm is linear.** For one axis, group the variants by the canonical assignment with
that axis removed. Two variants differ only in that axis exactly when they fall in one group. Then
compare inside each group. That is one pass over the variants for each axis, so the cost is `A × V`
and not `A × V²`. Compute the grouping once for each axis, and share it across every entry point.

**Compute it once for each cook.** Pass the result from `FinalizeModule` to `GenerateDedupeReport`.

### 6b. Every compiled variant is deep copied, and the copy is never read

`src/CookerDriver.cpp:544`:

```cpp
out_module_variants.push_back(variant);      // copy
out_variants.push_back(std::move(variant));  // move
```

`out_module_variants` is `moduleVariants`, and the two round trips read it. `out_variants` is
`variants`, declared at `RunCookOnce` line 774. Nothing reads it. It is passed to `CookModule` and
then goes out of scope.

So each cook copies every entry point's target text one extra time, and holds the copy until the cook
ends. At tier C that is 432 MiB of memory and 432 MiB of copy, for nothing.

**Correction: delete the parameter.** Neither the copy nor the vector has a reader.

#### Fixed. Parameter removed. Variant moved into container that is actually used.

### 6c. `FindCompiledVariant` is a linear scan inside a loop over variants

`src/CookerDriver.cpp:192`. `VerifyLibraryRoundTrip` and `VerifyLayoutRoundTrip` each call it once for
each variant, and it scans the whole span. That is `V²` comparisons for each check, so `2V²` for each
cook. At tier C that is 16.6 million.

The comparison is one `uint32`, so this is smaller than 6a. It is also easier to correct. The variants
arrive in ascending index order in both containers, so a shared index or one `std::ranges::lower_bound`
removes it.

#### Fixed. Variant address found in log2(n) time using lower_bound

### 6d. The interner copies the payload even when the payload is already in the table

`ContentInterner::Intern` at `include/ContentInterner.hpp:91` takes `PayloadType payload` by value. So
`AppendVariantToModule` at `src/CookedLibrary.cpp:173` copies the whole target text of every entry
point of every variant, and then discards the copy whenever the text is already interned.

At tier C that is 54 720 string copies of about 8 KiB, and the dedupe estimate says that most of them
are discarded. Call it 400 MiB of copy that reaches no table.

**Correction: add a `const PayloadType&` overload that copies only on the append path.** Keep the
by-value overload for a caller that can give up its copy.

### 6e. Provenance holds two strings for each intern call, and keeps them

`ProvenanceRecord` holds `EntryPointName` and `VariantDescription` as `std::string`. Each call to
`Intern` builds one, and the interner keeps it in `origins` until the freeze.

`AppendVariantToModule` builds one record for each entry point and each of three interners, plus one
for each resource. `VariantDescription` is the same text for every entry point of one variant.

| Tier | Provenance records | Strings kept |
|---|---|---|
| C | about 220 000 | about 440 000 |
| D | about 32 000 000 | about 64 000 000 |

**Correction: hold indices.** The variant index is already in the record. Add an entry point index and
remove both strings. A report that needs the text reads it from the module tables, which hold it once.

### 6f. `AttributesOfBinding` allocates and copies for every binding

`src/ResolveStage.cpp:185`. It walks every size attribute of the variant, for every binding of the
variant, and returns a fresh vector of **copied** attributes. Each copy holds a `std::vector<std::string>`.

The volume tiled forward module declares about 40 annotated resources. So one variant costs 40 vector
allocations, 40 full scans, and about 40 string vector copies.

`ExtractRawBindings` writes the attributes in ascending binding index order, so they are already
grouped. One pass with a moving cursor and a `std::span` removes the whole function.

### 6g. `ResolveVariant` copies the target text rather than moving it

`src/ResolveStage.cpp:223`: `entryPoint.Code = raw_entry_point.TargetText;`.

The raw variant is discarded directly after this call, unless `--dump-stage=raw` asked for it. So the
copy is needed only in the dump case. `UniformMembers`, `UsedBindingIndices`, and `Raster` copy the
same way.

**Correction: take the raw variant by value and move out of it.** Let the caller copy first, and only
when the dump asked for the raw model.

### 6h. `LinkVariant` rebuilds three strings for each axis of each variant

`src/SlangCompiler.cpp:775`. `MakeVariantModuleName`, `MakeVariantModulePath`, and
`MakeExportedConstantSource` all run for each axis of each variant. The result depends only on the
axis and the value, so the set of distinct results is small: it is the sum of the axis lengths.

Slang caches a module by name, so the second `loadModuleFromSourceString` for one name is cheap. The
string construction is not cached.

At tier C with 10 axes that is 86 400 string builds. This is the smallest item on the list. Build the
table once, next to the space, and index it.

### 6i. Two small items

- `src/SlangCompiler.cpp:770`. `components` is copy constructed from `BaseComponents`, and then
  `reserve` runs. The copy already sized the vector exactly, so the reserve forces a second
  allocation. Reserve first, then insert.
- `ContentInterner::Intern` calls `std::forward<PayloadType>` on a by-value parameter in the disabled
  path. It compiles to a move and it is correct. It reads as a forwarding reference, and it is not
  one. Write `std::move`.

### 6j. What the survey did **not** find

**The session, the module, and the entry point handles are shared across every variant.** `Impl` holds
`GlobalSession`, `Session`, `RootModule`, `BaseComponents`, and `EntryPointHandles`. `Initialize`
fills them once. `LinkVariant` reuses them and adds only the constant modules.

The regression that an earlier conversion introduced is not present. No cook repeats the session, the
module load, or the entry point collection for each entry point.

---

## 7. Order of work

| Step | Work | Depends on |
|---|---|---|
| 1 | Correct 6a. Group by masked assignment, and compute once for each cook | nothing |
| 2 | Correct 6b. Delete the unread parameter and its copy | nothing |
| 3 | Extract `RawVariantSource`, and move `SlangCompiler` behind it | nothing |
| 4 | Write the capture format, and `--capture-raw` | step 3 |
| 5 | Write `ReplayVariantSource`, and `--replay-raw` | step 4 |
| 6 | Add the live against replay comparison test | step 5 |
| 7 | Correct 6c to 6i, and measure each one with the harness | step 6 |

Steps 1 and 2 come first, because 6a will hide every other measurement the harness produces. A tier C
profile taken before step 1 says only that `InfluenceOfAxis` is slow.

Step 7 is last on purpose. The harness is the instrument. Correct the remaining items with the
instrument in place, and record the before number and the after number for each one.

---

## 8. Effect on the variant explosion plan

`docs/permutation-explosion-plans.md` holds the tier plan. Three statements change.

**Sections 1b, 1c, 1e, and 3 of that plan are complete.** The shaders are WGSL clean, the defects are
corrected, the shared modules are joined, and the debug passes are ported. See
`docs/vtf-shader-handoff.md`. Sections 1a, 2, and 4 remain.

**Tier C is the profiling workload.** It has 2880 variants and 19 entry points, it fits in memory,
and it loads every table. Capture it once, then profile the replay.

**Tier D never compiles, and that is the design.** The variant budget must reject it at stage 2. So
tier D produces no capture and needs none. It is a negative test for the budget, and the harness does
not apply to it.

One item joins the prerequisite list in section 1a of that plan. The budget correction and the axis
name correction were already there. Item 6a joins them, because tier C cannot be measured until it is
corrected.
