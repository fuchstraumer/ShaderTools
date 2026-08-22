# Add VolumeTiledForwardShading to the cook as a variant stress test

## Status, 2026-08-18

Read this before you read the rest.

| Section | State |
|---|---|
| 1a, cooker defects | open. Both confirmed again on 2026-08-19. Item 6a of `docs/replay-harness-and-cook-cost.md` joins this list |
| 1b, WGSL blockers | **complete.** Every construct is corrected |
| 1c, port defects | **complete.** Each correction carries a `fix:` comment |
| 1d, GLSL defects | **complete** |
| 1e, structural gaps | **complete.** The modules are joined and the empty files are written |
| 2, axes | open. The constants exist in `VtfConfig.slang`. No axis is registered |
| 3, debug rendering | **complete.** 19 entry points compile |
| 4, the shading shader and the tiers | open. This is the remaining work |

`docs/vtf-shader-handoff.md` records what the shaders now do, and what no test proves.
`docs/replay-harness-and-cook-cost.md` specifies the profiling harness that tier C needs.
`docs/shader-agent-handoff.md` records the BVH measurements, and three optimizations that failed.

## Context

Today the cooker cooks one module. That module is `OceanFft`. It has three axes, 35 variants, and an
index space of 56. This shows that the pipeline runs. It does not show where the pipeline breaks.

The volumetric tiled forward shaders were ported to Slang. Nobody registered, compiled, or tested
them. Their GLSL progenitors are in `tests/assets/volumetric_forward/`.

The goal is to give that port real load. Register it. Give it axes. Then increase the variant count
from tens to hundreds of thousands on purpose. The budget path, the layout interner, the influence
matrix, and the dedup report must each get sufficient load to fail.

Two decisions are made. The port becomes WGSL-clean. It uses typed views in a `ParameterBlock` and no
raw pointers. I report defects and do not correct them. The lists below are the deliverable. The
author writes the corrections.

One correction to the request. The GLSL sources contain no subgroup operations. A case-insensitive
search for `subgroup|ballot|shuffle|wave` in `tests/assets/volumetric_forward/` finds nothing. No
divergent path was lost. Each GLSL reduction is a shared-memory `barrier()` loop. The Slang port
added the wave intrinsics and removed the LDS path. Thus the task is not to restore a fallback. The
LDS path is the original. Put it back next to the wave path. Then put an axis on the pair. Two files
still contain the LDS path. It is written and unreachable.

**Terms used the same way throughout.** A *defect* is an error in code. An *axis* is a cooked
permutation axis. A *constant* is a Slang `[SpecializationConstant]`. A *wave* is a hardware SIMD
group. AMD calls it a wavefront and NVIDIA calls it a warp. This document says wave every time.

---

## 1. Findings, reported only

### 1a. Cooker defects that block this work

These defects are in `src/`. They are not in the shaders. Each one is in the path of module
registration.

**`VerifyAxisNamesAreDeclared` fails only on two or more undeclared axes.**
[PermutationSpace.cpp:464](src/PermutationSpace.cpp:464) sets `int32_t undeclaredCount = -1;`.
[PermutationSpace.cpp:490](src/PermutationSpace.cpp:490) tests `if (undeclaredCount > 0)`. One
undeclared axis increments the count to `0`. The cook then returns `Success`. The diagnostic still
prints. This check exists to catch one misspelled axis name. One misspelled axis name is what it
permits. To register VTF, the author must write ten to twenty new axis names by hand against handoff
rule 6.

**The cooker applies the variant budget after each variant compiles.** `CheckVariantBudget` at
[DedupeReport.cpp:378](src/DedupeReport.cpp:378) takes a `CookedModule`. `EnforceModulePolicy` runs
at [CookerDriver.cpp:492](src/CookerDriver.cpp:492), after stages 3 to 7. `EnumerateVariants` at
[CookerDriver.cpp:513](src/CookerDriver.cpp:513) never receives the policy. A module of 400,000
variants against a budget of 4096 does not fail early. It compiles 400,000 variants first. **Tier D
in section 4d cannot run until this check moves.** Phase E section 5 states the rule. It says to
apply `MaxVariants` during the walk and not after. The least costly form is a size test on
`VariantSet::Variants` directly after stage 2. It needs `FindPolicyForModule` moved a few lines
earlier. This is the one prerequisite.

### 1b. Constructs that WGSL does not permit

Each construct stops the module from cooking against the current target.

| Construct | Location | Reason WGSL rejects it |
|---|---|---|
| `PointLight* p`, 16-pointer resource structs | all Vtf files | buffer device address, no WGSL equivalent |
| `uniform uint8_t *clusterFlags` | `VtfSmallEntrypoints.slang:28` | no 8-bit storage type |
| `bool Enabled` in the three light structs | `VtfTypes.slang:11,26,36` | WGSL forbids `bool` in host-shareable address spaces |
| `uniform bool updateUniqueClusters` | `VtfSmallEntrypoints.slang:91` | same reason, and it is an axis |
| about 20.5 KiB of `groupshared` | `VtfAssignLightsToClustersBVH.slang:24-35` | 4 KiB stack plus 8 KiB plus 8 KiB against a 16 KiB limit |
| exactly 16384 B of `groupshared` | `VtfMergeSort.slang:13`, `VtfReduceLightAABBs.slang:13` | at the limit, no headroom |
| barrier in non-uniform control flow | `VtfReduceLightAABBs.slang:59-72` | `GroupMemoryBarrierWithGroupSync` inside `if (localInvocationIndex < numWaves)` |

A `float3` in `groupshared` has a 16 B stride. Thus `gs_AABBMin[512]` plus `gs_AABBMax[512]` is
16384 B. It is not the 12288 B that the declaration suggests.

### 1c. Correctness defects in the Slang port

The list starts with the defect that does the most damage.

1. **Each lane runs the aggregating atomic.** See `VtfAssignLightsToClustersBVH.slang:131` and
   `:194`. The call `gs_*LightCount.add(numActiveLights)` has no guard. The value
   `numActiveLights` is wave-uniform. Therefore each lane runs the atomic. The counter advances by a
   factor of the wave size. Each lane then receives a different `baseOffset`. The correct form is
   already written and never called. See
   [VtfSmallEntrypoints.slang:13](tests/assets/compute/VolumeTiledForwardShading/VtfSmallEntrypoints.slang:13).
   `AtomicAddWaveAggregated` guards with `WaveIsFirstLane` and then broadcasts with
   `WaveReadLaneFirst`.
2. **The wave64 ballot correction reached one of two identical paths.** Lines `:124-127` count all
   four components of `WaveActiveBallot`. A comment there names the reason. Line `:190` counts
   `activeLaneMask.x` only. Above 32 lanes, the point light count is too low.
3. **The two paths use different barriers.** Line `:147` uses `GroupMemoryBarrier()`. Its twin at
   `:210` uses `GroupMemoryBarrierWithGroupSync()`. Line `:149` then reads `gs_ParentIdx` in a race.
4. **A stack underflow writes index −1.** `PopNode` at `:66` calls `gs_StackPtr.sub(1)` with no
   guard. The pointer becomes negative. The next `PushNode` at `:57` writes `gs_NodeStack[-1]`.
   `PushNode` also increments on overflow and does not restore. Thus the pointer increases without
   limit and the traversal stops early without a message. `PopNode` returns `0`, which is also the
   exit sentinel of the loop.
5. **`WaveIsFirstLane()` stands for "thread 0 of the group"** at `:142`, `:205`, `:227`, `:251`, and
   `:270`. This holds only while the group size equals the wave size. This assumption is what a
   wave-size axis breaks. It is the reason that you cannot add the axis without other changes.
6. **A cross-wave LDS race.** `VtfReduceLightAABBs.slang:41-49` reads
   `gs_AABBMin[localInvocationIndex]` and then writes `gs_AABBMin[waveIndex]`. No barrier separates
   them. Wave 1 can overwrite the slot that wave 0 has not yet read.
7. **The `_NoWaveOps` radix sort does not sort.** `VtfRadixSort.slang:46` has an inverted ternary.
   The expression `localInvocationIndex == 0u ? gs_IsCurrentSortKeyBitFalse[localInvocationIndex - 1u]
   : 0u` reads index −1 for lane 0. It gives zero to each other lane. The exclusive scan does not
   work. This defect is important because that entry point is the control arm.
8. **The code treats `SV_GroupIndex` as a global index.** See `VtfSmallEntrypoints.slang:32` and
   `:56`, and `VtfComputeMortonCodes.slang:46`. Each group processes the same first 1024 elements.
   The incorrect comment that causes this is at `VtfComputeMortonCodes.slang:42`.
9. `VtfMergeSort.slang:170-191`. `SerialMerge` reads `gs_Keys[a0]` past `a1` on the last iteration.
10. `VtfBuildBVH.slang:167`. `NumLevelNodes[childLevel - 1]` underflows when `childLevel` is `0`.
11. `VtfBuildBVH.slang:85`. It sets `uint lightIdx = -1;` where the point-light twin uses `0`.
12. `ddCommonFunctions.slang:52`. `FresnelSchlick` takes a `float3 F0` and returns a `float` from
    `F0.x`. Colored metal fresnel does not work.
13. `ddCommonFunctions.slang:11`. `DoNormalMapping` calls `mul(TBN, v)`. Line `:19` builds `TBN` from
    rows. This is the inverse of the intended basis.
14. **The tree uses two matrix conventions.** `ClipToView` calls `mul(vector, matrix)`.
    `VtfUpdateLights` calls `mul(matrix, vector)`. One of the two is wrong.
15. `VtfFunctions.slang:23-24`. `ComputeClusterIndex3D` divides the screen position by the screen
    size. The result is 0 or 1. It needs pixels for each cluster.
16. A comment gives `SpotLight` as 96 bytes. Its members total 92 bytes.
17. Two files hold the same 32-ary tree with different lengths. `VtfBuildBVH.slang:8-30` has 7
    entries. `VtfAssignLightsToClustersBVH.slang:14-22` has 6 entries. A tree of depth 7 indexes the
    second table out of bounds.
18. `ddTonemapping.slang:19`. The function `float Reinhard` returns `inColor / (inColor + 1.0f)`,
    which is a `float3`. **This does not compile.** The orphaned file hides the defect.

### 1d. Defects that came from the GLSL

`ReduceLightsAABB.comp:67` uses `min` where it needs `max`. The Slang port corrected this.
`RadixSort.comp` scans with `>` where Hillis-Steele needs `>=`.
`AssignLightsToClustersBVH.glsl:83` and `:151` exchange the point and spot level counts.
`AssignLightsToClusters.comp:79-83` writes spot lights at point-light offsets. Both GLSL files set
`FLT_MAX` to `3.402823466e+36`, which is too small by a factor of 100.

### 1e. Structural gaps

- `materials/ddMaterials.slang` holds one line, `module ddMaterials;`. It has no `__include`.
  `ddMaterialsTypes.slang` declares `implementing ddMaterials;`. Therefore no consumer can reach
  `SurfaceOutput`, `LightingResult`, or `LightingInput`. `ddCommon.slang` omits `ddTonemapping.slang`
  in the same way.
- `common/ddFullscreenQuad.slang` has 0 bytes.
- **No `Material` struct exists in the Slang tree.** The material model exists only as YAML resource
  group declarations in `volumetric_forward.yaml`.
- Unreachable code: all of `VtfFunctions.slang`, `ClusterData`, `AtomicAddWaveAggregated`,
  `LogStepReduction`, `gs_KeyDestinationIdx` (1 KiB of unused LDS), `Cone`, `Frustum`, and the PBR
  block in `ddCommonFunctions.slang`.
- Entry point visibility and names are not consistent. The file uses `public`, `internal`, and the
  default. `VtfSmallEntrypoints.slang:44` writes `[Shader("compute")]` with a capital letter. Some
  entry points have the `Vtf` prefix and some do not.
- Missing ports: `ComputeClusterAABBs.comp`, which writes the `clusterAABBs` that each other shader
  reads. Also `AssignLightsToClusters.comp`, which is the brute-force arm. Also all 13 raster
  shaders.

---

## 2. Permutation axes

### 2a. The mechanism gap

Each knob in VTF is a `[SpecializationConstant]`. The cooker drives nothing through specialization
constants. It generates a one-line module for each axis value. That module holds
`export static const uint NAME = value;`. The Slang linker then matches the name to an
`extern const static` declaration. See `LinkVariant` at
[SlangCompiler.cpp:767](src/SlangCompiler.cpp:767). The text generators are at
[PermutationSpace.cpp:611](src/PermutationSpace.cpp:611). **VTF contains no `extern const static`.**
Therefore each axis below needs this conversion first:

```slang
[SpecializationConstant]                    →    extern const static uint VTF_REDUCTION_TYPE = 0u;
internal const uint ReductionType = 0;
```

The `= default` initializer does two jobs. It lets the module compile alone. It also lets
`CollectExternConstantDefinitions` read the default, so that a `[vx_*]` size expression can name an
undriven constant. The conversion also changes the binding time. A specialization constant binds at
pipeline creation. An `extern const static` binds at cook. Phase F section 3 calls this the earliest
sound binding time. An axis that sizes `groupshared` is a cook-time axis and stays one.

### 2b. Axes to declare

The groups below use the Phase F axis kinds, because the kind selects the mechanism.

**Capability. This varies by device and not by target, so it is Lodestone's work under the Phase E
section 1b rule.**

| Axis | Values | Notes |
|---|---|---|
| `VTF_USE_WAVE_OPS` | `false, true` | selects the technique |
| `VTF_WAVE_SIZE` | `16, 32, 64, 128` | `ActiveWhen VTF_USE_WAVE_OPS`, today `Parent` plus `RequiredParentValue` |

This gives 5 real combinations over 8 index slots. It has the same shape as
`IFFT_USE_WAVE_OPS` and `IFFT_WAVE_SIZE`. Therefore it needs no new machinery.

**Tuning**

| Axis | Values | Constraint |
|---|---|---|
| `VTF_REDUCTION_TYPE` | `0, 1` | from lights, or from AABBs |
| `VTF_BUILD_STAGE` | `0, 1` | bottom, or upper |
| `VTF_SORT_PASS` | `0, 1` | partitions, or merge. `SortAlgorithm` is the wrong name today |
| `VTF_MAX_LIGHTS_PER_CLUSTER` | `256, 512, 1024, 2048` | LDS limit. 2048 already exceeds the WGSL limit |
| `VTF_NODE_STACK_LIMIT` | `256, 512, 1024` | LDS |
| `VTF_MORTON_BITS` | `10, 16, 21` | written as a literal at `VtfComputeMortonCodes.slang:43` |
| `VTF_RADIX_BITS` | `30, 32` | literal at `VtfRadixSort.slang:21`. 30 agrees with 10-bit Morton and removes two passes |
| `VTF_MERGE_VALUES_PER_THREAD` | `4, 8` | its product with the thread count must stay at or below 4096 for 16 KiB |

**Technique**

| Axis | Values | Notes |
|---|---|---|
| `VTF_LIGHT_ASSIGN` | `BruteForce, Bvh` | needs `AssignLightsToClusters.comp` ported |
| `VTF_MERGE_PATH_MEMORY` | `Buffer, Shared` | today a runtime `bool` at `VtfMergeSort.slang:106`. Both call sites give it a literal |
| `VTF_SORT_CURVE` | `Morton, Hilbert` | added 2026-08-19. Nobody wrote the Hilbert encode yet |

`VTF_SORT_CURVE` is the safest new axis, and it is also the most useful. It changes the text of
`VtfComputeMortonCodes` and it is inert everywhere else, so the influence matrix has a true statement
to make. It also gives an A/B arm for the leaf quality measurements in `docs/shader-agent-handoff.md`.
No host code reads the value. See section 5a of that document for why a Hilbert curve targets the
worst leaf boxes, and section 6 for the measurement that must exist before anybody tunes the key.

**Resource presence.** Section 4 covers this kind, because it produces the variant growth.

### 2c. What a wave-size axis needs first

You cannot declare this axis alone. Three things hold the wave width at 32 today.

1. `AssignLightsToClustersNumThreads` is the thread count, the wave width, **and the BVH branching
   factor** at the same time. `GetFirstChild()` multiplies the parent index by it at
   `VtfAssignLightsToClustersBVH.slang:74`.
2. `NumChildNodes`, `NumLevelNodes`, and `FirstNodeIndex` are literal 32-ary tables in two files.
3. `WaveIsFirstLane()` stands for "thread 0 of the group" in five places.

**Recommendation. Separate the branching factor from the wave size.** Hold `VTF_BVH_BRANCHING` at 32.
It is a property of the data structure. CPU code shares it. It must not vary with the GPU. Then let
`VTF_WAVE_SIZE` vary alone. Replace `WaveIsFirstLane()` with `localInvocationIndex == 0` where the
code means group scope. Compute the node tables from the branching constant at link time instead of
writing them as literals.

Without that separation the axis gives a false result. It emits different text for wave 64 while the
traversal still walks a 32-ary tree.

**Added 2026-08-19. The branching factor and the leaf size are also one number, and they need not be.**
`VTF_BVH_BRANCHING` sets how many children an internal node holds. It also sets how many lights a leaf
holds. `ReduceBranchingSegment` in `VtfBuildBVH.slang` already reduces over aligned runs, so the leaf
build can take a run that is narrower than the wave. Measured: 4 lights for each leaf take the median
leaf diagonal from 0.356 to 0.199, and the tree depth stays at 3 levels.

**But this is a different kind of axis, and it needs a design first.** `VTF_SORT_CURVE` changes shader
text only. `VTF_BVH_BRANCHING` and a leaf size axis both change what the **host** must compute: node
counts, level offsets, and dispatch sizes. So the host must learn the cooked value at run time. The
manifest can carry it. Nobody must add either axis to `k_ModuleSpaces` before that design exists. This
is the Phase F binding time idea with a real example attached.

### 2d. How to restore the divergent path

There are three categories. Each needs a different quantity of work.

| Shader | State | Work |
|---|---|---|
| `VtfReduceLightAABBs` | `LogStepReduction` is written and correct, but **never called** | put the axis around the call site at `:163` and `:189` |
| `VtfRadixSort` | both paths exist as **two entry points** | let `VtfRadixSort` select on the axis. Keep `VtfRadixSort_NoWaveOps` as a reference entry point that stays *inert* to both wave axes. Assert that with `ExpectedAxisInfluence`, as `IfftPermuteCS` does |
| `VtfBuildBVH` | wave only. The comment at `:56` records the removal of the LDS pool | port the LDS reduction from `BuildBVH.comp:41-52` |
| `VtfAssignLightsToClustersBVH` | wave only, for the ballot compaction | port the shared `atomicAdd` append from `AssignLightsToClustersBVH.glsl:107,175` |
| `VtfMergeSort`, `VtfComputeMortonCodes` | never had a wave path | optional. Add one only if the axis must reach them |

The influence matrix does useful work here. When both arms exist, `ComputeAxisInfluence` measures
whether the wave axes change the output of each entry point. A shader that declares the axis and
emits the same text for both arms has a defect, and the matrix names it.

**Is wave size worth a variation? Yes, and for a stronger reason than the additional operations.**
The AMD wave64 argument is correct but small. The stronger reason is this.
`VtfAssignLightsToClustersBVH` allocates its LDS light list for each *group*. The ballot compaction
writes one entry for each *wave*. Those two scopes hold the same number today. That is why nobody has
found the lane and thread confusion in section 1c item 5. A wave width that varies makes the cook
emit text that differs. The defect then becomes visible instead of hidden.

---

## 3. How to close the gaps for debug rendering

The goal is a package that can render debug output. The order below follows the dependencies and not
the size of each item.

**Write the producers first. Nothing renders without them.**

1. **`VtfComputeClusterAABBs`.** Port `compute/ComputeClusterAABBs.comp`. It writes the
   `clusterAABBs` buffer that `AssignLightsToClusters` already reads. It is also the only consumer of
   `ClusterData` and of the three functions in `VtfFunctions.slang`. The port makes that unreachable
   code live. It also corrects the `ComputeClusterIndex3D` divisor defect.
2. **`VtfClusterSamples`**, a fragment shader. Port `ClusterSamples.frag`. It writes the cluster
   flags buffer that `VtfFindUniqueClusters` reads. The YAML declares `ClusterFlags` as `r8ui`. Under
   WGSL it becomes a `RWStructuredBuffer<uint>` with one flag for each element. That change also
   removes the `uint8_t*` blocker.
3. **`VtfDefaultVS` and `VtfDrawIdVS`.** Port `Default.vert` and `DrawID.vert` into one module. Make
   the draw index an axis. Under Vulkan, `gl_DrawID` needs `GL_ARB_shader_draw_parameters`. WGSL has
   no equivalent. Therefore, under a WGSL target, the draw index must arrive as an instance-step
   vertex attribute or as a uniform. This is a difference between targets. It belongs in a target
   profile and not in an axis.

**Then write the debug passes.**

4. **`VtfDebugClusters`**, a `.vert` and a `.frag`. **Remove the geometry stage.** WebGPU has no
   geometry shaders. The `.geom` expands one point into a 16-vertex box. An instanced draw with 16
   vertices for each instance does the same work. Let the vertex shader read `SV_VertexID % 16` as
   the box corner and `SV_VertexID / 16` as the cluster. This is the one place where I recommend a
   change instead of a port.
5. **`VtfDebugLights`**, a `.vert` and a `.frag`. `DoPointLights` is one specialization constant in
   two stages, and the author must keep the two copies equal by hand. As one cooked axis on one
   module it cannot drift. Correct the scale defect during the port. `Range * (Position + position)`
   also scales the position. It needs `Position + Range * position`. Also make `vInstanceID` agree.
   The vertex stage declares `int` and the fragment stage declares `uint`.
6. **`VtfDebugTexture`.** Both files have 0 bytes. Write them, or remove them. Nothing refers to them.

`ddFullscreenQuad.slang` is the correct location for the debug-texture blit. It is empty today.

**Correct the orphaned modules before any of this work.** `ddCommon.slang` must `__include`
`ddTonemapping`. `ddMaterials.slang` must `__include` `ddMaterialsTypes`. `ddTonemapping.slang:19`
must compile. Each raster shader depends on those three files. Therefore this is the first commit and
not a later cleanup.

---

## 4. The shading shader, and how to increase its variant count

### 4a. Why this shader suits the task

`Clustered.frag` gates ten optional textures on an index value of −1. Phase F section 4 calls this
the Indexed access model. That model makes resource presence a runtime test, so that no variant is
needed.

**WGSL has no descriptor indexing.** A `BindlessTextureArray[16384]` has no WGSL form. Each texture
must be a separately bound `Texture2D`. Therefore, under the Bound access model, those ten runtime
tests have only one place to go. They become permutation axes.

This is not a workaround. It shows clearly that the access model sets the axis count. The target
forces the variant growth. The test does not invent it.

The shader also loads the part of the repository with the least protection. Each presence axis
changes which `Texture2D` the entry point reads. Slang then removes the unreferenced global.
Therefore **the emitted binding layout differs for each variant.** Handoff rule 3 states that the
layout table has no second opinion. `CheckManifestLayout` compares the manifest against the table
that produced it. The WGSL binding scanner is the one check that reads layouts independently. This
shader gives that check hundreds of distinct layouts. The 21-layout `OceanFft` case cannot apply that
load.

### 4b. Shape

Add a module `VtfClusteredShading` under `tests/assets/render/ClusteredShading/`. Build it from
`Clustered.frag` and the YAML material declaration. The material becomes two typed views plus the
textures for each axis. Put all of them in one `ParameterBlock`. Use no pointers and no bindless
arrays.

```slang
struct MaterialParameters      // from volumetric_forward.yaml:47-74, the tinyobjloader layout
{
    float3 Ambient;  float Shininess;
    float3 Diffuse;  float Ior;
    float3 Specular; float Dissolve;
    float3 Emissive; float Roughness;
    float  Metallic; float Sheen;
    float  ClearcoatThickness; float ClearcoatRoughness;
    float  Anisotropy; float AnisotropyRotation; float HeightScale;
    uint   Illum;     // int in the GLSL. uint keeps the 16-byte rule correct
};

struct ShadingResources
{
    StructuredBuffer<MaterialParameters> Materials;
    StructuredBuffer<PointLight>         PointLights;
    StructuredBuffer<uint2>              PointLightGrid;   // was an untyped uint* at [i*2], [i*2+1]
    StructuredBuffer<uint>               PointLightIndexList;
    ConstantBuffer<ClusterData>          Clusters;
    ConstantBuffer<CommonMatrices>       Matrices;
    // one texture for each MAT_HAS_* axis
    Texture2D<float4> AlbedoMap;
    Texture2D<float3> NormalMap;
    SamplerState      LinearRepeat;
};
ParameterBlock<ShadingResources> Resources;
```

Use `SurfaceOutput`, `LightingResult`, and `LightingInput` from `ddMaterialsTypes.slang` after that
module becomes reachable. Also use `DistributionGGX`, `GeometrySchlickGGX`, `FresnelSchlick`,
`MakeTBN`, and `Attenuate` from `ddCommonFunctions.slang`. Each of those is unreachable today. Each
one was written for this shader.

### 4c. The axes

| Axis | Values | Kind | Constraint |
|---|---|---|---|
| `MAT_HAS_ALBEDO_MAP` | `false, true` | resource presence | |
| `MAT_HAS_NORMAL_MAP` | `false, true` | resource presence | |
| `MAT_HAS_BUMP_MAP` | `false, true` | resource presence | `ActiveWhen !MAT_HAS_NORMAL_MAP`. The original is an `else if` chain |
| `MAT_HAS_METALLIC_MAP` | `false, true` | resource presence | |
| `MAT_HAS_ROUGHNESS_MAP` | `false, true` | resource presence | |
| `MAT_HAS_AO_MAP` | `false, true` | resource presence | |
| `MAT_HAS_EMISSIVE_MAP` | `false, true` | resource presence | |
| `MAT_HAS_SPECULAR_MAP` | `false, true` | resource presence | |
| `MAT_HAS_ALPHA_MAP` | `false, true` | resource presence | |
| `MAT_HAS_DISPLACEMENT_MAP` | `false, true` | resource presence | |
| `SHADING_ALPHA_MODE` | `Opaque, Mask, Blend` | technique | |
| `SHADING_BRDF` | `Lambert, GgxSmith, GgxCharlie` | technique | |
| `SHADING_LIGHT_SET` | `Point, PointSpot, All` | technique | |
| `SHADING_TONEMAP` | `None, Reinhard, Lottes` | technique | agrees with `ddTonemapping.slang` |
| `SHADING_PARALLAX_STEPS` | `8, 16, 32` | tuning | `ActiveWhen MAT_HAS_DISPLACEMENT_MAP` |
| `SHADING_DEBUG_VIEW` | `Off, ClusterIndex, LightHeatmap` | technique | adds the light-count heatmap that the GLSL lacked |
| `VTF_USE_WAVE_OPS` | `false, true` | capability | |
| `VTF_WAVE_SIZE` | `16, 32, 64, 128` | capability | `ActiveWhen VTF_USE_WAVE_OPS` |

The two dependent axes have the most effect on the index. `VTF_WAVE_SIZE` gives 5 real combinations
over 8 slots. `SHADING_PARALLAX_STEPS` gives 4 over 6. Therefore the hole ratio compounds by
multiplication. The mixed-radix index space then grows faster than the variant count. Phase E section
6 predicts this result. Tier C below makes it measurable.

### 4d. The tiers, and how to reach a chosen count

Each tier adds axes to the tier above it. Therefore you write the module registration once. The tier
is then a question of which axes the space names. **Real** excludes the holes from dependent axes.
**Slots** is the mixed-radix index space, which is what the emitted table costs today.

| Tier | Axes | Real | Slots | Purpose |
|---|---|---|---|---|
| **A** | `ALPHA_MODE`(3) × `HAS_NORMAL_MAP`(2) | **6** | 6 | shows that the module registers and cooks. No wave axes and no dependents |
| **B** | 4 presence(16) × `ALPHA_MODE`(3) × `BRDF`{Lambert,Ggx}(2) × wave(5/8) | **480** | 768 | a few hundred. First real load on the layout interner |
| **C** | 6 presence(64) × `ALPHA_MODE`(3) × `BRDF`(3) × wave(5/8) | **2 880** | 4 608 | a few thousand. The hole ratio is 1.6 and the dedup report shows it |
| **D** | 10 presence(1024) × `ALPHA_MODE`(3) × `BRDF`(3) × `LIGHT_SET`(3) × `TONEMAP`(3) × wave(5/8) | **414 720** | 663 552 | over budget on purpose |

Tier D arithmetic. The independent axes give `1024 × 3 × 3 × 3 × 3 = 82 944` combinations. Multiply
by 5 real wave combinations to get 414 720 variants. Multiply by 8 wave slots to get 663 552 index
slots. `SHADING_DEBUG_VIEW`(3) and `SHADING_PARALLAX_STEPS` together multiply that by about three
again.

**How to make Tier D fail usefully.** Set `MaxVariants` to 4096. Correct the enforcement point first,
as section 1a describes. Without that correction the cook attempts 414 720 compilations before it
reports the problem. At the `OceanFft` rate that takes several days. With the check at stage 2 the
cook fails in under one second and names the module. Phase E section 5 specifies that behaviour. It
is the only thing that makes Tier D a test instead of a hang.

**Keep Tier D as a negative test in the repository.** No test covers a budget failure today. A
`CookTest`-style target that expects a non-zero exit code and the budget diagnostic costs little,
after the check moves. It is the control arm for the whole policy mechanism.

**Recommended order: A, then B, then C as the committed default. Keep D behind a flag.** Tier C has
2 880 variants, which is about 80 times `OceanFft`. It must still cook in an acceptable time. It is
the tier to leave in `ctest`. Measure Tier B before you commit to Tier C. If entry point codegen and
not linking dominates the cost for each variant, then Tier C may need `--single-threaded` off and a
longer test timeout.

### 4e. What to measure during the growth

- **Unique layouts.** `OceanFft` interns 21 layouts from 1 placement, 3 usage masks, and 7 sizes.
  This product occurs because `ReflectedBinding` carries three concerns with three different keys.
  See Phase D section 4b. Presence axes load that structure directly. If the layout count grows in
  proportion to the variant count and does not settle, then step D8b is more urgent than it appears.
- **Hash collisions.** The interner counts each collision and never resolves one without a report. At
  Tier C the source table is large enough to make FNV-1a 64 worth measurement. This is the first
  workload that could justify the xxHash submodule.
- **`--no-dedupe` stays correct.** It is the control arm. At Tier C it emits the text of each variant
  separately. Both arms must reach the same axis influence. `DedupeInfluenceTest` holds that line.
- **`--verify-deterministic`.** It cooks twice into two `MemoryOutputSink` objects. At Tier C that
  doubles a large cook. Expect this flag to find any unordered container that remains in the path.

---

## 5. Order of work

1. **Correct the cooker prerequisites.** Correct `undeclaredCount`. Move the `MaxVariants` check to
   a point directly after `EnumerateVariants`. No other step is safe before this one.
2. **Correct the shared modules.** `ddCommon` includes `ddTonemapping`. `ddMaterials` includes
   `ddMaterialsTypes`. `ddTonemapping.slang:19` returns `float3`.
3. **Make VTF WGSL-clean.** Change pointers to a `ParameterBlock` with typed views. Change `bool` to
   `uint`. Change `uint8_t` to `uint`. Reduce each LDS budget below 16 KiB. Move the barrier out of
   divergent control flow.
4. **Change `[SpecializationConstant]` to `extern const static`.** Then register
   `VolumeTiledForwardShading` in `k_ModuleSpaces` with the tuning axes from section 2b. This gives
   the first real cook.
5. **Add the wave axes.** Separate the branching factor from the wave size first, as section 2c
   describes. Then restore the LDS arms.
6. **Add the producers and the debug passes**, as section 3 describes, so that the package renders.
7. **Add `VtfClusteredShading`** at Tier A, then Tier B, then Tier C. See section 4.

Steps 1 and 2 are prerequisites. Steps 3 and 4 give a module that cooks. Steps 5, 6, and 7 do not
depend on each other. You can do them in any order.

---

## 6. Verification

Each step uses a check that the repository already has.

- **Build.** Run `cmake --build build/ninja-msvc --config Debug`. Read the exit code of the build
  itself. A pipe into `tail` gives you the exit code of `tail`.
- **The current suite stays green.** Run
  `ctest --test-dir build/ninja-msvc -C Debug --output-on-failure`. The nine current targets must
  pass at each step. Steps 1 and 2 change shared code.
- **The `OceanFft` numbers do not move.** They are 35 variants over a space of 56, 105 entry point
  variants, 77 unique sources, 21 unique layouts, and 0 hash collisions. A change after step 1 means
  that the budget check or the axis check changed behaviour that it must not change.
- **For each step, on the new module.** The cook returns 0. `--verify-deterministic` passes.
  `--no-dedupe` emits valid output and reaches the same axis influence.
- **`--dump-stage` is the regression harness.** Record the `space` and `variants` dumps as goldens
  when a tier lands. Then require byte-identical dumps across any refactor that must not change the
  space. Phase D built the dump for this purpose.
- **The cross-check is the real proof for section 4.** `ValidateVariantReflection` reads
  `@group` and `@binding` out of each emitted WGSL. It then compares them against the bindings of the
  entry point. Presence axes change the binding set for each variant. Therefore a non-zero mismatch
  count is the signal that resource presence and reflection disagree.
- **Add a negative test.** Add a Tier D target that expects a non-zero exit code and the budget
  diagnostic. Register it with `add_lodestone_unit_test(... TEST_ARGS ...)`, as `CookTest` does.