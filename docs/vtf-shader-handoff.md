# VTF shader handoff

Written 2026-08-18. This document records the state of the volume tiled forward shaders. Read it
before you continue that work.

**Scope of this document.** It covers `tests/assets/` and `tests/scripts/` only. It does not cover
the C++ cooker.

**Terms.** A *check* is one assertion in a test script. A *defect* is an error in code. An *axis* is
a cooked permutation axis. A *constant* is an `extern const static` value. An *arm* is one value of
a two-value axis. A *wave* is a hardware SIMD group.

---

## 1. What changed

The Slang port of the volume tiled forward shaders now compiles and runs. Before this work it did
neither.

| Change | Files |
|---|---|
| Raw pointers became `ParameterBlock` with typed views | every `Vtf*.slang` |
| `[SpecializationConstant]` became `extern const static` | `VtfConfig.slang` |
| Shared resource declarations moved to one file | `VtfBindings.slang` |
| The shared-memory arm returned next to the wave arm | `VtfReduceLightAABBs`, `VtfBuildBVH`, `VtfAssignLightsToClustersBVH`, `VtfRadixSort` |
| Missing passes were ported from GLSL | `VtfComputeClusterAABBs`, `VtfClusterSamples` |
| Debug passes were ported | `VtfDebugClusters`, `VtfDebugLights`, `VtfDebugTexture` |
| Orphaned modules joined their parent module | `ddCommon.slang`, `ddMaterials.slang` |
| A fullscreen triangle filled an empty file | `ddFullscreenQuad.slang` |

New files: `VtfConfig.slang`, `VtfBindings.slang`, `VtfComputeClusterAABBs.slang`,
`VtfClusterSamples.slang`, `VtfDebugClusters.slang`, `VtfDebugLights.slang`,
`VtfDebugTexture.slang`, `tests/scripts/vtf_support.py`, `tests/scripts/test_vtf_compute.py`,
`tests/scripts/vtf_visualize.py`.

Each correction carries a comment that starts with `fix:`. Search for that prefix to find them.

---

## 2. How to run the tests

Run the GPU checks:

```bash
cd tests/scripts && py test_vtf_compute.py
```

Use `py`, which is Python 3.14. It holds slangpy, numpy, and matplotlib. `python` is a different
interpreter and it holds none of them.

Compile one entry point to WGSL:

```bash
build/ninja-msvc/third_party/slang/slang-2026.14.1-windows-x86_64/bin/slangc.exe -target wgsl -I tests/assets -I tests/assets/common -I tests/assets/materials -I tests/assets/render -I tests/assets/compute/VolumeTiledForwardShading -entry VtfRadixSort -stage compute tests/assets/compute/VolumeTiledForwardShading/VolumeTiledForwardShading.slang -o out.wgsl
```

---

## 3. What the tests prove

The last run gave 33 checks passed and 0 failed, on an NVIDIA RTX 4080 through D3D12.

| Entry point | Compared against | Arms run |
|---|---|---|
| `VtfRadixSort` | numpy sort, and the other arm | wave, shared memory |
| `VtfRadixSort_NoWaveOps` | numpy sort | shared memory only, on purpose |
| `VtfMergeSort` | numpy sort, after four merge passes | both passes |
| `VtfReduceLightAABBs` | numpy min and max, and the other arm | wave, shared memory |
| `VtfComputeMortonCodes` | a float32 numpy reference | one arm |
| `VtfBuildBVH` | numpy boxes over each run of 32 lights | leaf stage, upper stage |
| `VtfComputeClusterAABBs` | shape rules on the grid | one arm |
| `VtfAssignLightsToClusters` | a brute force sphere and box test | wave, shared memory |

Every numpy reference shares no code with the shader it checks.

---

## 4. What the tests do not prove

Read this section before you state that the port works.

1. **No raster pass has drawn a pixel.** The eight raster entry points compile, and nothing more.
2. **Spot lights are untested.** Each test supplies zero spot lights.
3. **Three compute entry points are untested.** They are `VtfFindUniqueClusters`, `VtfUpdateLights`,
   and `VtfUpdateClusterIndirectArgs`.
4. **`VTF_REDUCTION_TYPE` value 1 is untested.** Only the reduction from the light list runs.
5. **Wave widths above 32 are untested.** The test device holds 32 lanes. The 64 and 128 arms
   compile, and no device ran them.
6. **The `SerialMerge` bounds guards are not proven.** Section 7 gives the detail.

---

## 5. Facts that are easy to lose

### The SlangPy binding trap

Each VTF entry point takes `uniform ParameterBlock<T> resources`. That is an entry point parameter.
`ComputeKernel.dispatch(vars=...)` cannot reach an entry point parameter. The call raises no error,
the dispatch runs, and every resource reads as zero.

Use `LinkedKernel.dispatch` in `vtf_support.py`. It goes through
`ShaderCursor(shader_object).find_entry_point(0)`, and that path raises on a wrong name.

`test_ifft.py` uses the `vars=` form. OceanFft declares module-scope globals, so that form works
there. The two shapes need two different binding paths.

### The permutation override

`VtfDevice.constant_module` builds a one-line Slang module that holds
`export static const <type> <NAME> = <value>;`. The test links it next to the real module. This is
the same mechanism that `MakeExportedConstantSource` in `src/PermutationSpace.cpp` uses. Pass
`constants=` to `VtfDevice.kernel` to cook one variant.

### The wave size contract

`VTF_WAVE_SIZE` is an **upper bound**, not an exact width. A variant cooked for 128 lanes is correct
on a device of 32 lanes. A variant cooked for 32 lanes is **not** correct on a device of 64 lanes.
Only the ballot count in `VtfAssignLightsToClustersBVH` reads the constant. Every other algorithm
reads `WaveGetLaneCount()` and is correct at any width.

### The branching factor is not the wave width

`VTF_BVH_BRANCHING` is 32 and must stay 32. It is a property of the tree that CPU code shares.
`VTF_WAVE_SIZE` varies alone. The two were one number before this work, and that is why a wave size
axis could not exist.

### Reference material

| Source | Location |
|---|---|
| The thesis, as markdown | `C:\Users\fuchs\Downloads\VolumeTiledForwardShading.md` |
| Frame sequencing, and the merge loop | `D:\DiamondDogs\modules\VtfModule\src\vtfTasksAndSteps.cpp` |
| The merge sort loop | the same file, line 4023 |
| The radix sort dispatch | the same file, line 2775 |
| GLSL progenitors | `tests/assets/volumetric_forward/` |

The thesis holds the pseudocode for each algorithm. Algorithm 7.1 and Algorithm 7.2 confirm that the
shared-memory reduction and the `tid = 0` pop are the published design.

---

## 6. Defects found in the C++ cooker

Another agent owns `src/`. I did not correct these two defects. Report them again if nobody has.

1. **`VerifyAxisNamesAreDeclared` accepts one undeclared axis.**
   `src/PermutationSpace.cpp:464` sets the counter to −1, and line 490 tests for a value above zero.
   One undeclared axis raises the counter to zero, and the cook returns success.
2. **The variant budget applies after every variant compiles.**
   `CheckVariantBudget` in `src/DedupeReport.cpp:378` takes a `CookedModule`.
   `EnumerateVariants` at `src/CookerDriver.cpp:513` never receives the policy.

The second defect blocks any test that must exceed a variant budget.

---

## 7. One claim I corrected

I first reported the `SerialMerge` bounds guards in `VtfMergeSort.slang` as a defect correction. A
mutation test removed the guards, and the suite still passed 32 of 32.

The loop guard already stops the stale value from reaching the output. The read goes past the
thread's own range, and it stays inside the shared array in every case the test reaches. The guards
are hardening, and they are not a proven correction. Keep them, because the read is undefined by
specification. Do not call them a defect fix.

---

## 8. The test must be able to fail

Each correction in this work carries a mutation test. The method is:

1. Return the defect to the shader.
2. Run the suite.
3. Confirm that the check which covers that defect fails.
4. Return the shader to its corrected state.
5. Run the suite again.

This method found a weakness in the test itself. A check that read "the radix sort orders all 16
chunks" passed while the offset was wrong, because an array of zeros is sorted. The suite now also
checks that each chunk holds the keys of its own input range.

Do not add a check without a mutation that makes it fail.

---

## 9. Next steps, in order

| Step | Work | Blocked by |
|---|---|---|
| 1 | Draw one raster pass and capture the image | nothing |
| 2 | Add spot lights to each compute test | nothing |
| 3 | Test the three untested compute entry points | nothing |
| 4 | Test `VTF_REDUCTION_TYPE` value 1 | nothing |
| 5 | Correct the two cooker defects in section 6 | the agent who owns `src/` |
| 6 | Register `VolumeTiledForwardShading` in `k_ModuleSpaces` | step 5 |
| 7 | Add the shading shader, and raise the variant count | step 6 |

Steps 1 to 4 need no C++ change. Start there.

Step 7 follows the plan in `C:\Users\fuchs\.claude\plans\i-would-like-you-piped-bumblebee.md`. That
plan gives four tiers, from 6 variants to 414720 variants.

---

## 10. Rules for the next session

1. Change files under `tests/assets/` and `tests/scripts/` only.
2. Do not change `src/`, `include/`, or `client/`. Another agent owns them.
3. Do not change `docs/shader-cooker-handoff.md`. It has uncommitted edits.
4. Write each comment and each document in ASD-STE100.
5. Give each subagent prompt the same register.
6. Report what a test proves, and report what it does not prove.
