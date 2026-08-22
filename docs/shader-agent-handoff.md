# Shader agent handoff

Written 2026-08-19. This document is for the agent that works on shaders and on test scripts.

A different agent works on `src/`, `include/`, and `client/`. That agent reads
`docs/replay-harness-and-cook-cost.md`. This document does not repeat it.

**Scope.** This session changed two files, and both are test scripts. It changed no shader. It
changed no C++ file.

**Read first.** `docs/vtf-shader-handoff.md` records the state of the shaders. Section 4 of that
document lists what no test proves. That list did not change.

**Terms.** A *leaf* is one node of the BVH that holds lights and no child nodes. A *leaf diagonal* is
the length of the diagonal of one leaf box. The *scene diagonal* is the same length for the box that
holds every light. A *slice* is one depth step of the cluster grid. A *centre* is a light position
with no radius. A *sphere* is a light position plus its radius.

---

## 1. What changed in the tree

| File | Change |
|---|---|
| `tests/scripts/vtf_visualize.py` | The lower left panel now draws three GPU buffers |
| `tests/scripts/test_vtf_compute.py` | `test_merge_sort` returns those three buffers to the caller |

The suite gives 33 checks passed and 0 failed after the change. That count did not move.

### Why the panel changed

The panel called `np.sort(codes)` and then drew the result. The plot sorted the keys itself. So it
drew a monotone ramp for a correct sort and for a defective sort. It could not fail.

The panel now draws three buffers that the GPU wrote:

1. the unsorted input, which is the control
2. the output of `VtfRadixSort`, which is 16 chunks
3. the output of `VtfMergeSort`, which is one ramp

A mutation test confirmed the change. The mutation made `GetChunkBase` return 0. Four checks failed.
The orange line ramped one time across the first 256 slots and stayed at zero for the other 3840
slots. The author restored the shader, and the suite returned to 33 checks passed.

The leaf box histogram moved to the lower right panel. It was the `else` arm of a test that no caller
satisfied.

---

## 2. Two claims from this session that were wrong

Correct these if you read them in an earlier message.

### The leaf boxes are much better than the first report said

The first report said the leaf boxes were 1.75 times the ideal size. That comparison was wrong. It
compared boxes that bound light **spheres** against an ideal for light **centres**.

| Measurement | Median leaf diagonal over scene diagonal |
|---|---|
| centres only | 0.223 |
| equal split ideal, 128 boxes | 0.198 |
| as the shader builds them, spheres | 0.356 |

The Morton partition is 1.13 times the ideal. There is almost nothing to gain from a better
partition. The step from 0.223 to 0.356 is light radius, and nothing else.

### The two locality measurements do not agree

The first report said the Morton walk ratio and the leaf box ratio both gave about 1.8 times ideal.
That agreement was an artifact of the error above. The two numbers do not agree, and no conclusion
follows from them together.

---

## 3. What the BVH measurements say

Every number below comes from `make_lights(4096)` in `tests/scripts/vtf_support.py`.

### Light radius controls the leaf size

Mean light range is 4.49. The scene extent is 94.7 by 94.9 by 53.1.

| Change | Median | Max |
|---|---|---|
| as built | 0.356 | 0.903 |
| radii at half | 0.281 | 0.840 |
| radii at zero | 0.223 | 0.781 |
| 8 lights for each leaf | 0.243 | 0.819 |
| 4 lights for each leaf | 0.199 | 0.753 |

Two readings follow.

1. Smaller radii give the largest gain, and that gain costs no code. It is content policy.
2. Neither change moves the maximum. The worst box stays near 0.78 with radius at zero.

### The z axis is loose, but not for the reason it appears

The z axis is the loosest axis. It has the largest mean extent, 0.413 against 0.404 for x and 0.350
for y. It is the largest axis in 66 leaves of 128. One leaf covers about 12 slices of the 24 the grid
holds.

The cause is geometry, and not resolution. The z axis is the short axis of the scene. A radius of
4.49 therefore covers a larger fraction of z than of x.

### The worst boxes are x and y, and not z

| Diagonal | x | y | z |
|---|---|---|---|
| 0.903 | 0.973 | 0.911 | 0.591 |
| 0.715 | 0.940 | 0.452 | 0.556 |

These two boxes hold runs of lights that cross a high bit boundary of the Morton code. They are
Z-order straddles. Light radius does not cause them.

---

## 4. Three experiments that failed

Do not repeat these. Each one sounds correct and each one gives nothing.

### 4a. Quantize z in the log space the cluster grid uses

The cluster grid divides depth exponentially. The Morton code divides depth linearly. So the two
disagree about which lights are near in depth.

| Arm | Mean slices for each leaf |
|---|---|
| linear z, as today | 11.82 |
| log z, grid aligned | 11.71 |

The result is no gain. The maximum became worse. Light radius covers a large part of the log range
near the near plane, so the warp gives back what it wins in the far field.

### 4b. Give z more bits, 10/10/12

The proposal was to add two low bits of z to the code.

| Arm | Median | Max | z extent |
|---|---|---|---|
| 10/10/10, today | 0.356 | 0.903 | 0.413 |
| 10/10/12, extra z bits at the bottom | 0.356 | 0.874 | 0.411 |
| 10/10/12, z spread through the schedule | 0.361 | 0.803 | 0.415 |

Extra low bits do nothing, and they must do nothing. The module holds 4096 lights in a space of
2^30 cells. That is one light for each 260000 cells. The low bits separate only lights that share one
cell, and no lights share one cell.

The top bits decide which lights fall in one leaf. A leaf covers about 1/128 of the code range, so
the top 7 bits decide it. The z axis already takes 3 of those 7 bits, because z is the highest axis
of each group of three. The x axis and the y axis take 2 each.

The third row is the important one. It gives z 12 splits through the whole schedule. The z extent
moves from 0.413 to 0.415. More splits do not make z tighter. This proves that the z looseness is
radius, and not resolution.

### 4c. The direction is the opposite of the proposal

| Arm | Median | Max | z extent | Slices |
|---|---|---|---|---|
| 10/10/10, today | 0.356 | 0.903 | 0.413 | 11.82 |
| 10/10/8, z spread, fewer bits | 0.345 | 0.781 | 0.494 | 13.60 |

Fewer z bits give the best median and the best maximum on this data.

The reason is the normalization. The code maps each axis to the range 0 to 1, whatever its true size.
So one z bit covers 53.1/2^k world units, and one x bit covers 94.7/2^k. The z axis already holds
1.8 times more resolution than x in world units. The extra bits do more work on x and on y.

**The bit budget must follow the physical extent, and not the importance of the axis.**

Do not apply this. The 10/10/8 arm makes the slice count worse, from 11.82 to 13.60. The two
measurements disagree, so neither one is the true cost. See section 6.

---

## 5. Proposals that stay open

### 5a. Replace the Morton curve with a Hilbert curve

A Hilbert curve has better locality than a Z-order curve. It has no long jump at a bit boundary. So
it is the one change that targets the worst boxes in section 3.

The encode does not need recursion. It is a bounded loop over the bit count, which is 10 steps. Each
step reads a 3-bit octant, maps it through a gray code, and updates a small rotation state. It uses
integer operations only. It reads no memory. Every lane runs the same 10 steps, so it causes no
divergence.

Nobody measured a Hilbert curve on this data. Treat the gain as a property of the curve, and not as a
result.

### 5b. Make the curve an axis

Declare `VTF_SORT_CURVE` with the values `Morton` and `Hilbert`. Two results follow.

1. It gives a control arm. Both curves cook, both run, and a test compares them on one input.
2. It adds true variants to the explosion plan. The axis changes the text of one entry point, and it
   is inert everywhere else. So the influence matrix has a real statement to make.

This axis is safe. It changes shader text only. No host code reads it.

### 5c. Separate the leaf size from the branching factor

`VTF_BVH_BRANCHING` is 32. It sets two different things today:

- how many children each internal node holds
- how many lights each leaf holds

These do not need to be one number. `ReduceBranchingSegment` in `VtfBuildBVH.slang` already performs
a segmented reduction over aligned runs. That function is what a leaf build needs when the leaf is
narrower than the wave. This is not a rewrite.

The measurement in section 3 says 4 lights for each leaf gives a median of 0.199.

The tree depth does not increase. At 32 children for each node, 4096 lights give 128 leaves, then 4
nodes, then 1 root. That is 3 levels. At 4 lights for each leaf, 4096 lights give 1024 leaves, then
32 nodes, then 1 root. That is 3 levels again, because 1024 is 32 squared. The usual objection to
small leaves does not apply at this light count.

---

## 6. A rule for the next measurement

Two metrics appeared in this session, and they disagree:

- the leaf diagonal over the scene diagonal
- the count of depth slices for each leaf

Neither one is the true cost. The true cost is the count of clusters that overlap one leaf box,
because that is what the traversal pays. A cluster is not a cube. Its shape changes with depth.

**Do not tune the sort key until that measurement exists.** It needs the projection matrix, and the
raster work supplies it. Step 1 of section 9 in `docs/vtf-shader-handoff.md` is therefore a
prerequisite for section 4 and section 5a of this document.

No profile in this session measured traversal time. Every number here states quality, and no number
states cost.

---

## 7. Two kinds of axis

This distinction came out of the leaf size discussion. It is the Phase F binding time idea with a
real example.

**An axis that stays inside the shader.** `VTF_SORT_CURVE` is one. It changes emitted text. No code
outside the shader learns the value.

**An axis that the host must also know.** `VTF_BVH_BRANCHING` is one. The host computes node counts,
level offsets, and dispatch sizes from it. `docs/vtf-shader-handoff.md` states that it must stay 32
for this reason. A leaf size axis has the same problem in a smaller form, because it changes the
dispatch the host issues.

The second kind needs a design first. The manifest can carry the value, and that is probably the
correct answer. It is not a table entry, and nobody must add one before the design exists.

---

## 8. The cook cost work

`docs/replay-harness-and-cook-cost.md` holds the full list. The author corrected two items during
this session, and marked each one in that file under a `####` heading.

| Item | State |
|---|---|
| 6a, `ComputeAxisInfluence` is quadratic and runs twice | **open.** This is the largest item |
| 6b, every variant is deep copied and never read | fixed |
| 6c, `FindCompiledVariant` is a linear scan | fixed, with `lower_bound` |
| 6d to 6i | open |

Item 6a stays first. At tier C it costs tens of seconds. At tier D it does not finish. A profile of
tier C before that correction reports only that `InfluenceOfAxis` is slow.

The two cooker defects in section 1a of `docs/permutation-explosion-plans.md` are also open. This
session confirmed both:

- `src/PermutationSpace.cpp:464` still sets the counter to −1, and line 490 still tests for a value
  above zero.
- `CheckVariantBudget` still takes a `CookedModule`, so the budget applies after every variant
  compiles.

---

## 9. Next steps, in order

| Step | Work | Blocked by |
|---|---|---|
| 1 | Draw one raster pass and capture the image | nothing |
| 2 | Add spot lights to each compute test | nothing |
| 3 | Test the three compute entry points that no test runs | nothing |
| 4 | Measure clusters overlapped for each leaf | step 1 |
| 5 | Implement the Hilbert curve, and compare it against Morton | step 4 |
| 6 | Correct item 6a, and the two cooker defects | the agent who owns `src/` |
| 7 | Register `VolumeTiledForwardShading` in `k_ModuleSpaces` | step 6 |

Steps 1 to 3 are the same three steps the earlier handoff lists. They did not move, because this
session changed no shader.

Step 4 is new. It is the measurement that section 6 asks for.

---

## 10. Rules for the next session

1. Change files under `tests/assets/` and `tests/scripts/` only.
2. Do not change `src/`, `include/`, or `client/`. A different agent owns them.
3. Do not change `docs/shader-cooker-handoff.md`. It holds edits that nobody committed.
4. Write each comment and each document in ASD-STE100.
5. Give each subagent prompt the same register.
6. Report what a test proves. Also report what it does not prove.
7. Do not add a check without a mutation that makes it fail.
8. Establish that a cost exists before you correct it. This session produced three failed
   optimizations, and each one looked correct before the measurement.
