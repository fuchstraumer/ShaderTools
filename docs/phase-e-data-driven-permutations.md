# Phase E: data-driven permutations

Phase E replaces the compiled-in permutation registry with declarations in the shader source and policy
in a data file. It is the next large increase in capability, and it must come after Phase D.

Read `docs/phase-d-stage-separation-plan.md` first. Section 9 of this document lists the changes Phase D
must make for Phase E, and those changes cost almost nothing if you make them at the time.

---

## 1. Why Phase E comes second

Phase E changes the enumeration, the index, the declaration site, and the policy source. Each one of
those changes the emitted output. Today you can only compare four artifacts, and every one of them is
far downstream of the code you touch.

Phase D gives `--dump-stage`. After Phase D, a change to stage 1 shows up as a diff of the stage 1 dump.
That turns each Phase E step from "cook and hope" into "cook and compare".

Phase E also divides much better after Phase D. The index change alone touches three places (section 6).
With stage dumps, each of the three can move on its own and prove itself.

---

## 1b. What Phase E does not have to build

`docs/phase-f-vocabulary.md` divides the work between Slang and this repository. Read §6 of that file
before you start Phase E, because it removes work from this plan.

The short form. An axis has a **kind**, and each kind gets one mechanism:

| Axis kind | Mechanism | Owner |
|---|---|---|
| Capability that varies **by target** | `[require()]`, `__target_switch`, capability aliases | **Slang** |
| Capability that varies **by device** | A cooked axis, chosen at run time | **Lodestone** |
| Technique | `interface` and generic specialization | **Slang**, driven by a Lodestone axis |
| Tuning | A cooked axis, or a specialization constant | **Lodestone** |
| Resource presence | The access model, and a null or −1 test | **Lodestone**, through the target profile |

The rule that separates the two capability rows:

> **A capability that varies between targets belongs to Slang. A capability that varies between
> devices inside one target belongs to Lodestone.**

Slang's capability atoms resolve at cook time against a fixed target profile. They state what a target
*language* can express, never what a *device* supports. `subgroup_basic` includes `wgsl`
(`slang-capabilities.capdef:2504`), yet WebGPU subgroups are an optional per-adapter feature. Wave
width is 32 or 64 on one Vulkan extension set.

So `IFFT_USE_WAVE_OPS` and `IFFT_WAVE_SIZE` stay Phase E axes. They are device questions, not
portability questions.

**Two consequences for this plan.** Capability atoms are a closed set, so a technique axis can never
be one; E7 stays as written. And a portability requirement should never become an axis, so an axis
that differs only between targets is a defect in the declaration, not a variant to cook.

---

## 1c. Found work: the entry point scope is never read

**Complete on 2026-08-21.** `tests/assets/EntryPointParams.slang` cooks, and the cross-check agrees.
`EntryPointParamsCookTest` runs it. Section 11 of `docs/phase-d-tail-handoff.md` records what the
step found: the placement was right on the first cook, and the emitted name carries a fixed
`entryPointParams_` prefix that `StripSlangNameMangling` now removes.

Found on 2026-08-20. A question about `ParameterBlock` uncovered it.

This section and §1d record gaps in what the cooker can see. §1e records a defect. None of the three
is a phase E idea. **Do all three first**, for three reasons:

- Each one is small.
- Each one is independent of every other step in this plan.
- All three touch the stage 3 code that E6 also changes.

This gap and §1d cannot ship a wrong layout. The cross-check reads the emitted text and fails the
cook. It also names each slot that reflection missed. The capability is absent, and it is not broken.
§1e is different, and §1e states the difference.

### What is missing

`SlangCompiler::Impl::ExtractRawEntryPoint` reads four things: the stage, the workgroup size, the
varying inputs and outputs, and the used binding mask. It never reads
`entryPointLayout->getTypeLayout()`. A `uniform` parameter on an entry point therefore produces no
binding.

A cook proved this. A probe module declares `Output` at global scope. The same module declares two
`uniform` parameters on the entry point. The cook failed and printed this:

    wgsl declares @group(0) @binding(1) entryPointParams_albedoMap : reflection has no binding at that location
    wgsl declares @group(0) @binding(2) entryPointParams_samplerState : reflection has no binding at that location

### The model already fits, so this needs no schema change

`RawVariant` holds one binding list for the variant. It also holds a `UsedBindingIndices` list for
each entry point. Step D8b keyed each resource per variant. It keyed each visibility list per variant
and entry point.

An entry point parameter is a resource that exactly one entry point sees. A visibility list already
states that fact. An entry point parameter is therefore one row in the variant's binding list, plus
one visibility entry.

This step adds no table. It changes no manifest record. It changes nothing in `CookedModule`.

### Steps

1. **Separate the binding range walk from `ExtractRawBindings`.** That function reads only
   `getGlobalParamsTypeLayout()` today. Give the new function a `slang::TypeLayoutReflection*` and a
   placement offset. The new function returns the drafts. This step changes no behaviour, and the
   `raw` stage dump of `OceanFft` must not move.
2. **Call the new function again for each entry point**, with `entryPointLayout->getTypeLayout()`.
   Add the offsets from the entry point var layout. The probe shows that an entry point parameter
   takes an offset in the space the global bindings use. `Output` took `@group(0) @binding(0)`, and
   `albedoMap` took `@group(0) @binding(1)`. Read each offset from reflection. Do not assume it.
   Compare each result against the emitted text.
3. **Ownership decides visibility for an entry point parameter. A placement query must not decide
   it.** `CollectUsedBindingIndices` asks `isParameterLocationUsed(DESCRIPTOR_TABLE_SLOT, group,
   binding)` of the metadata of one entry point. Two entry points can put different resources at the
   same group and binding, because Slang generates each entry point as its own artifact. A placement
   query then lets one entry point claim the parameter of a different entry point. An entry point
   parameter belongs to exactly one entry point by construction. Append its indices to that entry
   point directly, and keep the placement query for the global bindings.
4. **Append the new bindings. Do not sort the list again.** `ExtractRawBindings` sorts the global
   bindings by placement, and that sort is stable. Append the bindings of each entry point after the
   global bindings. Use the order in which the module declares the entry points, then the binding
   range order. A sort across the full list becomes wrong when two placements are equal.
   `--verify-deterministic` is the check.
5. **A `[vx_*]` attribute needs no new code.** `CollectRawSizeAttributes` reads the leaf variable, so
   it works for an entry point parameter without a change. Set `RawSizeAttribute::BindingIndex` in
   the pass that sets it for the global bindings, after the append.

### The acceptance test, and why it is a real one

The completion criterion is not "the code walks the tree". The criterion is this: **a probe module
with entry point parameters cooks, and the cross-check agrees.** That check reads the emitted text and
compares it against reflection. It is a second opinion, so a wrong walk cannot satisfy it. It fails
today, and that fact makes it a test.

Put the probe module in `tests/assets/`. Give `CookTest` a second module, or add a test target beside
it.

`OceanFft` declares no entry point parameter. Every existing golden file and every artifact must
therefore stay byte identical. That is the regression check for step 1.

*Medium. Low risk. The cross-check finds an error on the first cook.*

---

## 1d. Found work: `ParameterBlock` is invisible for a second reason

**Complete on 2026-08-21.** `tests/assets/ParameterBlocks.slang` cooks, and the cross-check agrees.
`ParameterBlocksCookTest` runs it. Section 13 of `docs/phase-d-tail-handoff.md` records the three
numbers as measured, and it corrects two claims below.

A probe found this beside §1c, and a second probe measured it on 2026-08-20. This gap is the other
half of the §1c gap. It is a separate step, because the correction goes in a different place.

### What is missing

A `ParameterBlock` at global scope also produces no binding. Slang gives a parent scope only
*descriptor ranges*, and never *binding ranges*
(`third_party/slang/source/slang/slang-reflection-api.cpp:2213`). The parent therefore sees one range
of type `ParameterBlock`, and that range has a `descriptorSetIndex` of -1. The guard in
`ExtractRawBindings` then drops the range in silence.

The contents of the block live in a sub-object range. `getSubObjectRangeCount()` reaches that range,
and no code in this repository calls it.

A parameter block on an entry point fails for this reason and for the §1c reason at the same time.

### A block is a set of bound placements, and the group is the space of the block

This design needs no new placement kind. The emitted WGSL is already flat. The contents of a block are
ordinary `@group` and `@binding` pairs. The group of each pair is the space that the block took.

A bare declaration stays in the default space, and it serves as a module resource or a root resource.
A block is an organizational tool that produces a set of its own. A block on an entry point states
that the set belongs to that entry point. All three forms compose, because all three are a group and a
binding.

### Every number is reachable, and two of the offsets are not the obvious ones

A probe module declares `ParameterBlock<Material>` with a texture, a sampler, and a `float4`. The same
module declares a global `Output`. The compiler emitted four bindings:

| Binding | Placement |
|---|---|
| `MaterialBlock` | `@group(1) @binding(0)` |
| `MaterialBlock_AlbedoMap` | `@group(1) @binding(1)` |
| `MaterialBlock_Sampler` | `@group(1) @binding(2)` |
| `Output` | `@group(0) @binding(0)` |

Reflection gives each number:

| Number | Source | Value in the probe |
|---|---|---|
| the group of the block | `getSubObjectRangeOffset(i)->getOffset(SUB_ELEMENT_REGISTER_SPACE)` | 1 |
| the binding of the container | `leafTypeLayout->getContainerVarLayout()->getOffset(DESCRIPTOR_TABLE_SLOT)` | 0 |
| the binding of each content | the descriptor range index offset of the element type layout | 1, then 2 |

Four traps follow. Each one costs a cook.

1. **`getSubObjectRangeSpaceOffset()` is not the group.** It returned 0 for a block that took space 1.
   Read the space from the offset var layout of the sub-object range.
2. **The constant buffer of the container is not a binding range.** Slang adds one to hold the
   ordinary data of the block, and a client must create it. The ranges of the parent do not hold it.
   The ranges of the element do not hold it. Only `getContainerVarLayout()` gives it. A walk that
   reads the sub-object ranges alone therefore misses one slot, and that slot carries the name of the
   block.
3. **The content indices already include the offset of the element.** The element var layout of the
   probe reports slot 1. The two content ranges report 1 and 2, and not 0 and 1. A walk that adds the
   element offset a second time moves every binding by one.
4. **A sub-object range is not always a block.** The probe reported two ranges, and the second range
   was the plain `Output` buffer. Read the binding type of the range that each sub-object names. Keep
   only the block types.

### Steps

1. Walk `getSubObjectRangeCount()` after the binding ranges of the parent. Keep only the ranges whose
   binding type is `ParameterBlock` or `ConstantBuffer`.
2. Give the constant buffer of the container a binding of its own. Give that binding the name of the
   block.
3. Walk into the element type layout again, with the space of the block as the base. A block inside a
   block takes a space of its own. The walk must add the spaces together, and it must not assume one
   level.
4. Confirm that the usage query still answers. The contents of a block have a real group and binding,
   so `isParameterLocationUsed` should answer for them. Check this. Do not assume it.
5. Delete nothing from `FromSlangBindingType` yet. It maps `BindingType::ParameterBlock` to
   `BindingKind::UniformBuffer`. No code reaches that line today, and the answer is wrong for a block
   of textures. After the walk lands, the block stops being a binding, and the container becomes the
   uniform buffer. The line then goes away, or it becomes correct by construction. Decide this with
   the code open.

### Two probes that nobody wrote yet

- **A block that holds only ordinary data.** No probe tests this. The block may take a space, or it
  may become one constant buffer in the space of the parent. The walk must handle each result.
- **Two entry points, and a block on each one.** The group and binding pairs can repeat across entry
  points. Step 3 of §1c names the same hazard.

### Why this is not phase F

`ResourcePlacement` is already a sum type, and `BoundPlacement` is one alternative. A pointer model or
a bindless model changes the placement of *every* resource, and a block adds nothing to that question.
Under the bound model the group number **is** the identity of the set. A flat list therefore loses
nothing that the emitted artifact expresses.

*Medium. Low risk. The cross-check names every slot that is missing or misplaced.*

---

## 1e. Found work: a pointer type reaches WGSL, and no check sees it

Found on 2026-08-20, during the phase F spike on buffer device addresses. This one is a defect, and
not a missing capability. A cook can exit 0 and write invalid output.

### What happens

A module declared `PointLight* Points` inside a push constant structure. `slangc` compiled that module
for WGSL and returned 0. The emitted text holds this:

```wgsl
struct Lights_std430_0
{
    @align(8) Points_0 : ptr<, PointLight_0>,
    @align(8) Count_0 : u32,
};
var<uniform> LightData_0 : Lights_std430_0;
```

WGSL has no such pointer form. A pointer may not appear in a host shareable structure. The address
space is empty as well. No WGSL implementation accepts this text.

### Why the cross-check misses it

`WgslBindingScanner` reads `@group` and `@binding`. The invalid declaration carries neither, so the
scanner never sees it. Every other binding agrees with reflection, and the cook exits 0.

No rule in `CLAUDE.md` covers this. The four validators compare the cooker against itself. **Not one
of them asks whether the emitted text is legal for the target.**

### The check to add

Reject a pointer type at stage 3, and compare it against the access model of the target profile.

`slang::TypeReflection::Kind::Pointer` is value 18. Stage 3 reads Slang, so the type kind is visible
there. The target profile states the access model. A `Pointer` kind under a `Bound` access model fails
the cook and names the resource.

Do this at stage 3, and not later. `ReflectedUniformMember` holds a name, an offset, a size, and an
array count. It holds no type kind, so no later stage can ask the question. `CLAUDE.md` states the
rule that decides the placement: validate at the ingestion surface, then trust the data inside.

### This is a validator, and not a defensive check

Two answers come from two places. The target profile states the access model, and the author chose it
on the command line. Slang states the type kind, and the shader source decided it. Neither side
derived the other, so the comparison finds a defect that neither side finds alone.

*Small. No risk. `OceanFft` declares no pointer, so no artifact moves.*

---

## 2. The two declaration mechanisms are not two ways to do one thing

You described interfaces and constants as two ways to define permutation data. They are not parallel.
They sit at different levels, and a single axis model must hold both.

| | Interface axis | Constant axis |
|---|---|---|
| What varies | A type, and the functions it carries | An integer or a boolean value |
| Slang mechanism | Generic specialization over an `interface` | `extern const static` link-time constant |
| Where the values come from | The set of types that conform | A list the author writes |
| Can arithmetic read it | No | Yes. `IFFT_SIZE * 4` is a size expression |
| Natural example | Which BRDF | How large the FFT is |

The difference reaches the size expression evaluator. An interface axis has no integer value, so
`EvaluateSizeExpression` cannot see it. The fix is small: **give every axis value an ordinal**. The
evaluator then sees the ordinal, the linker sees the type or the constant, and one axis model holds
both.

So the axis value gets a domain:

- `Boolean` — false, true
- `Integer` — a list of values
- `Enum` — an ordinal with a name, for a value that is a label and not a number
- `Type` — an ordinal plus a fully qualified Slang type name, for an interface axis

`PermutationValue` is `std::variant<bool, uint32_t, int32_t>` today. `Enum` and `Type` add a name, so
this type grows. `PermutationValueToInt64` keeps working: it returns the ordinal.

### Two more fields, for phase F

An axis also carries a **kind** and an **earliest sound binding time**. Both come from
`docs/phase-f-vocabulary.md`, §2 and §3, and both are two enum fields on `PermutationAxis`.

- `AxisKind` — resource presence, capability, tuning, or technique. Section 1b maps each kind to its
  mechanism.
- `EarliestBindingTime` — cook, pipeline, draw, or thread. The earliest point at which the axis is
  **correct**. An axis that sizes a `groupshared` array is cook time and can never be later. An axis
  that only selects a pointer is sound at draw time, and at every earlier time as well.

The author states the earliest. A target profile then decides how late the value really binds, and
Phase E's policy file (§8) is where that decision is written. This is what lets a client query a live
adapter, map it to a profile, and ask the manifest for a runnable variant, instead of parsing
extensions and maintaining every capability variant by hand.

Add both fields in E2. They cost two enums now. Retrofitting them means revisiting every axis
declaration.

### The hazard in interface axes

An interface axis takes its values from the types that conform to the interface. **Any module linked
later can add a conformance.** So the value set is "what the cooker saw at cook time", and adding a
new BRDF renumbers every variant of every shader that uses that axis.

Two defenses, and you want both:

1. **Sort conformances by fully qualified type name.** Never by discovery order. Discovery order comes
   from a file system walk and is not stable across machines.
2. **Let policy name the cooked set.** A new conformance then enters the space only when someone adds
   it to the policy file. That is the same rule as section 8, and it is why policy and declaration must
   stay apart.

---

## 3. Where the declarations live

Put the axis on the declaration, with an attribute, exactly as `[vx_element_count]` does.

```slang
[vx_axis_values("128, 256, 512, 1024")]
extern const static uint IFFT_SIZE;

[vx_axis_values("32, 64")]
[vx_axis_active_when("IFFT_USE_WAVE_OPS != 0")]
extern const static uint IFFT_WAVE_SIZE;
```

**The argument is one string, not a list of integers.** Slang attributes have fixed arity and no
optional parameters, so a four-value axis and a two-value axis would otherwise need two attribute
names. This is the same tax the repository already pays for size expressions, and it is paid for the
same reason. A string also leaves room for a range form later.

### What this buys

**`VerifyAxisNamesAreDeclared` becomes unnecessary, and rule 7 of the handoff becomes impossible to
break.** Today an axis name that does not match the `extern const static` name links a symbol nobody
references, leaves the shader on its default, and fails nowhere. That whole failure class disappears,
because the axis **is** the declaration. There is no second name to keep in step.

This is the strongest argument for Phase E. It is larger than the ergonomics.

You also get the inheritance you asked for. The bootstrap compile reads the dependency closure, so an
axis declared in `CommonLighting.slang` reaches every module that includes it, and a change there
propagates through the same machinery.

### The cost, and one rule that makes it sound

The cost is a bootstrap compile. The cooker compiles the module once with defaults, reads the axis
attributes, and only then enumerates. That is fine, and section 9 shows that Phase D almost gives you
this call for free.

The rule: **an axis declaration must be at module scope and must not be inside a conditional.** A
declaration the bootstrap compile cannot see is an axis the cooker never enumerates, and nothing
reports it. Reject a conditional declaration and name the file and the line.

### Inheritance is also the explosion vector

A common header that declares five axes multiplies into every module that includes it. This is the
Slipspace failure mode: a static branch somewhere central, and a combinatorial cost nobody sees until
the build is slow.

You already hold three defenses. Keep all three, and add a fourth.

| Mechanism | When | Exact | Cost |
|---|---|---|---|
| Canonicalization | Before the compile | No, a guess | Free |
| **Symbol reachability (new)** | Before the compile | No, conservative | One text scan |
| Interning | After the compile | Yes | Already paid |
| Influence matrix | After the compile | Yes, and it reports | Integer compares |

The new one: after the bootstrap compile, check whether the axis symbol appears anywhere in the source
text reachable from the module's entry points. If it does not appear, the axis cannot affect this
module, and the cooker drops it before it compiles anything. This is conservative, so it never removes
an axis that matters. The influence matrix still measures the axes that survive, and policy still
enforces the measurement.

---

## 4. The constraint language

Extend `SizeExpression` with a comparison level and a logical level. The grammar gains two rows below
`shift`:

```
logical    := comparison (( '&&' | '||' ) comparison)*
comparison := shift (( '==' | '!=' | '<' | '<=' | '>' | '>=' ) shift)*
shift      := sum (( '<<' | '>>' ) sum)*
```

`unary` gains `!`. The result stays `int64_t`, and nonzero means true. This matches C and Slang, adds
no type, and keeps one evaluator. A size expression and a constraint expression are then the same
function called in two places.

About 80 new lines. The 28 existing tests stay green.

### Two constraint kinds, and no more

Kconfig has `depends on`, `select`, `visible if`, and `range`. Take two of them.

- **`ActiveWhen`** — the axis takes part only when the expression holds. When it does not hold, the
  axis is inactive and canonicalization fills its first value. This is exactly today's `Parent` and
  `RequiredParentValue`, generalized from one parent to an expression.
- **`Require`** — an assignment is invalid when the expression fails. This prunes the enumeration.

**Do not implement `select`.** In kconfig, `select` forces a value and bypasses `depends on`. It is
the source of most kconfig defect reports, and it exists there for backward compatibility. You have no
such history.

### One requirement that is easy to miss

`ActiveWhen` expressions can name other axes, and those axes can have `ActiveWhen` expressions of
their own. **The axes therefore form a directed graph, and canonicalization must evaluate them in
topological order.** A cycle must fail when the registry loads, not during a cook, and the error must
name every axis in the cycle.

Today `Parent` is a single pointer and a cycle is nearly impossible to write by accident. With
expressions it is easy. Add the cycle check with the constraint language, in the same step.

---

## 5. SAT, #SAT, and why neither one is the problem

Your reading is right. Validity is SAT. Counting is #SAT. Both are hard in general, and both are
tractable here.

But the useful conclusion is stronger than "tractable":

**You do not need a counting oracle at all, because you already enumerate.**

Stage 2 walks the whole valid space today and produces every `VariantDescriptor`. A depth-first walk
with constraint propagation does the same job with constraints in place. Once you hold the sorted list
of valid assignments, the rank of an assignment is its position in that list. Counting was only ever
needed to compute a rank **without** the list.

Sizes, so the decision rests on numbers:

- 20 boolean axes give a nominal space of about one million.
- Constraints usually cut that to hundreds or low thousands.
- A depth-first walk with propagation visits the valid space plus the pruned branches. That is
  thousands of nodes, and each node is a few integer compares.

**Enumeration is never the bottleneck. Compilation is, by about five orders of magnitude.** A cook of
400 variants spends milliseconds enumerating and minutes compiling. This is why the policy file in
section 8 matters far more than the choice of algorithm.

One guard: **enforce `MaxVariants` during the walk, not after it.** A constraint set that admits ten
million assignments must fail in a second and name the module. It must not enumerate for an hour and
then report that the budget is exceeded.

---

## 6. The index: ranking, and the cheapest form of it

You are right that mixed radix stops working. A space with holes needs a million slots for four
hundred variants, and the emitted tables carry every hole.

Three forms, in order of cost.

### Form 1: a sorted key table and a binary search — **take this one**

Pack each canonical assignment into one `uint64_t`, using the existing mixed-radix arithmetic. Sort the
packed keys. **The dense index is the position in that sorted array.**

- Lookup: `std::lower_bound` over a `constexpr` array. About 9 steps for 400 variants, 17 for 100,000.
- Memory: 8 bytes for each variant. 400 variants cost 3.2 KiB, against 4 MiB of holes today.
- `constexpr`: yes. A sorted array and a binary search are both `constexpr` in C++20.

The property that makes this cheap: **canonicalization does not change at all.** The packed
mixed-radix key stays the canonical form. It stops being the storage index and becomes the search key.
Your partial-assignment lookup still works, because it works on the canonical form.

### Form 2: prefix counts and true ranking

Bake the number of valid completions of each prefix. Ranking is then a sum over the axes, with no
search:

```
rank = sum over axes i, of  sum over w < v_i, of  N(v_0..v_{i-1}, a_i = w)
```

This is O(axes) instead of O(log variants), and it needs the #SAT counts at cook time. It is the more
elegant answer and it is not worth it yet. Consider it if a module ever passes about 100,000 variants,
which no shader should.

### Form 3: mixed radix with the holes

What exists today. It fails exactly as you say.

### Three places the index is implemented

This is the finding that decides how Phase E divides. The mixed-radix index has three implementations,
and one of them is kept in step by hand:

| Site | What it is |
|---|---|
| `ComputeVariantIndex` in `src/PermutationSpace.cpp:414` | The cooker's own arithmetic |
| `EmitVariantIndex` in `src/ShaderLibraryEmitter.cpp:378` | Emits a `constexpr` C++ function |
| `EmitCanonicalize` in `src/ShaderLibraryEmitter.cpp:351` | Emits the `constexpr` canonical form |

The comment above `EmitVariantIndex` reads: "This must match ComputeVariantIndex in". That is a
hand-maintained duplication of the one rule the whole index rests on.

**Form 1 removes the duplication.** The emitted side becomes a sorted array plus a search, and the
search does not encode the radix rule. The cooker computes the keys; the header only looks them up.
The rule then lives in one place.

### The input to the index now has a type

Phase D step D8c gave the canonical form its own type, `CanonicalAssignment`. Only
`CanonicalizeAssignment` builds one, so a partial assignment cannot reach `ComputeVariantIndex` and
return a plausible wrong index.

Form 1 replaces the arithmetic, not the guarantee. Whatever computes a ranking index still needs an
input it can trust, so carry the shape forward. Drop the implementation if the sorted key table does
not want it.

### One invariant to write down now

**A dense variant index is stable only inside one cook.** Adding an axis, adding a value, or adding an
interface conformance renumbers it. This is true of mixed radix today and stays true under ranking, so
it is not a regression, but Phase E makes the space much easier to change.

State the rule where a reader will find it: **a material, a save file, or any asset outside the cook
must reference a variant by its assignment, never by its index.** The index is a cook-internal handle.

---

## 7. Interface axes: spike before you commit

I will not assert what the Slang version in `third_party/` supports here. The original design document
set the standard: it checked each mechanism against a commit and cited file and line.

**Spike E0** answers three questions against `third_party/slang`:

1. Can a module declare an `extern` value or type of interface type that link-time specialization
   fills, in the way `extern const static` works for a constant?
2. Can reflection enumerate the types that conform to an interface, and does it give a stable fully
   qualified name for each?
3. Can an interface carry its own bindings, so that choosing a conformance changes the binding set?
   Your note says interface objects can declare their own inputs. If so, the layout depends on the
   axis, and the layout interner already handles that.

Report with citations, as the feasibility section of the design document does.

**The fallback always works, so the spike decides ergonomics and not capability.** An interface axis
lowers to an `Enum` axis plus a dispatch function:

```slang
[vx_axis_values("Lambert, GgxSmith, CharlieSheen")]
extern const static uint BRDF_KIND;
```

with a factory that switches on it. Slang specializes at link time either way, so the compile cost is
the same. You lose the type-level statement, and you gain nothing else. If E0 answers yes, take the
interface form. If it answers no, take this and lose nothing but elegance.

---

## 8. Policy in a file

Declaring a space is authorship. Cooking a space is policy. Unity splits `multi_compile` from
`shader_feature` for this reason, and Unreal uses `ShouldCompilePermutation` for the same one. A
declared axis is not always a cooked axis.

**Format: JSON.** TOML reads better for a person editing by hand, and a good TOML parser is several
times the size. Avoid YAML: the design document already says so, and it is right.

### The reader: a third-party library, behind a facade

An earlier draft of this document said to hand-write the reader. **That was wrong, for a reason it did
not weigh: a tech artist edits this file.** Parse error quality is therefore a user-facing feature. A
library gives byte offsets and sensible messages. A quick hand-rolled parser gives whatever somebody
bothered to write, and the author of this repository is then the person who fields "it says invalid
and I do not know why".

A writer is easy to hand-write. A parser must handle escapes, Unicode, number edge cases, and every
malformed input a person can type. The two are not the same job.

**Do not name the library in this document.** Pick it at E5 against these criteria:

| Criterion | Why |
|---|---|
| No exceptions on the error path, or a non-throwing entry point | `JsonWriter.hpp` gives this as its own reason to exist. Every error path in this repository is `std::expected` |
| Small compile-time cost | A very large single header costs every translation unit that sees it. This repository already feels a 10-minute rebuild |
| Byte offset and a readable message on a parse failure | The tech artist is the user |

**Hide it behind a facade, whichever one wins.** A pointer-to-implementation reader with a small
key-and-value accessor keeps the third-party type out of every header, keeps the include in one
translation unit, and makes a later swap one file. This repository has never let a third-party type
above the lowest layer that needs it, and `SlangCompiler.hpp` is the standing example: it names no
Slang type.

### The writer and the reader should not be the same code

Keep `JsonWriter` for writing, and take the library for reading. Two independent implementations that
agree is **an asymmetry, not a redundancy** — the same shape as the WGSL scanner checking Slang's
reflection. A round trip through one hand-written implementation only proves it round-trips its own
defects.

Contents:

| Key | Meaning | Moves from |
|---|---|---|
| `MaxVariants` | The variant budget | `k_OceanFftPolicy` in C++ |
| `ExpectedInfluence` | Which axes must be inert for which entry point | `k_OceanFftPolicy` in C++ |
| `CookValues` | The subset of an axis's values to cook | new. This is `shader_feature` |
| `CookWhen` | A predicate that removes an assignment from the cook | new. This is `ShouldCompilePermutation` |

`CookValues` and `CookWhen` use the same expression evaluator as section 4. One grammar, three uses.

A note on your `todo.md` item about parameter domain and device properties: `CookWhen` is where that
lands. A predicate over a named platform profile removes assignments the target cannot run, and it
needs no new mechanism beyond a few symbols in the evaluator's table.

### Per-target sections

**The policy file needs a section for each target profile**, and this is where the lowering decision
from `docs/phase-f-vocabulary.md` §3 is written down. A section states, for each axis, the binding
time that target really uses. An axis whose earliest sound binding time is `Draw` stays a cooked axis
on a target with no push constants, and collapses to one variant on a target that has them.

`MaxVariants` then caps each target separately, which is the honest way to state the cost: one shader
can be four variants on the desktop target and forty on WebGPU.

### One field in the manifest, not in the policy

**Each variant must record the capability requirement it was cooked for.** A client queries the live
adapter, maps the result to a target profile, and asks the manifest which variants that profile can
run. Without the field, the client is back to parsing extensions by hand.

This is a small schema addition, and it is far cheaper to make while the manifest is already changing
in E4 than to add later. `ManifestVariant` gained two index fields in phase D step D8b, so the record
already moved once and nothing outside this repository reads it. The `sizeof` assertions that used to
pin every record are gone, and `k_IsManifestRecord` replaces them. See §10.

---

## 9. What Phase D should do for Phase E

These cost almost nothing during Phase D and save real work later. Numbers refer to
`docs/phase-d-stage-separation-plan.md` §6.

**Phase D is complete, so read this as a checklist against what landed.** D4, D5b, and D6 are done.
D-wide holds. D1 is half done. **D2 is open**, and it is the one this section called highest leverage,
so settle it before E2 rather than during it.

### D2 — dump the space stage with full fidelity. **Highest leverage of the five.**

Phase E's riskiest change is replacing `k_ModuleSpaces` with data. If the stage 1 dump has a stable JSON
shape after D2, then that job becomes "produce the same JSON from a different source", and the golden
dump of OceanFft is a byte-exact acceptance test for it.

So dump every field an axis has, not the ones the current type happens to hold: name, values,
constraints, and the value domain. Constraints are a single parent pointer today and the dump will be
dull. That is fine. The **shape** is what Phase E needs.

**Open. Phase D did not do this, and E2 pays for it.** `WriteAxis` in `src/StageDump.cpp` writes the
name, the values, the parent, and the required parent value. E2 adds `AxisValueDomain`, `AxisKind`,
and `EarliestBindingTime`, so the dump gains fields and cannot be byte identical to the phase D
golden.

Decide which before E2 starts, and say so in the step:

- Add the fields to the dump now as nulls, take the one artifact change, and keep the byte comparison
  as E2's evidence. This is the cheaper answer and it keeps the rule the whole phase rests on.
- Or let E2 move the dump on purpose and give it a field by field comparison instead, the way phase D
  step D8b had to.

The first one costs one edit. The second costs E2 its acceptance test.

### D4 — separate the module-level stage 3 call from the per-variant one

**Done.** `SlangCompiler::PrepareRawModule(space)` returns a `RawModule`, and
`SlangCompiler::CompileVariantRaw(descriptor)` returns a `RawVariant`. `ResolveExternConstantDefaults`
and `GetExternConstantDefaults` are gone: the defaults leave the compiler on the stage 3 output, and
stage 4 reads them from there. The bootstrap compile that step E6 needs is an attribute reader added
to `PrepareRawModule`, not a restructure.

Phase E's bootstrap compile is exactly "compile once with defaults and read attributes". That is
`CompileVariantRaw` with an empty assignment.

The plan already has `RawModule` carry the extern constant defaults, because stage 4 needs them. Go one
step further and give stage 3 **two** entry points:

- `PrepareRawModule()` → `RawModule` — module facts: entry point names, extern defaults, and later the
  axis declarations
- `CompileVariantRaw(const RawModule&, descriptor)` → `RawVariant`

If stage 3 exposes only the per-variant call, Phase E has to restructure it. If it exposes both, Phase E
adds an attribute reader to a call that already exists. This is the single change with the highest
ratio of later saving to present cost.

### D1 — name the JSON target for both directions

**Half done, and the other half is one line.** The alias is `lodestone::json`, which is right, but the
target behind it is `lodestone_json_writer`. `JsonReader.cpp` then lands in a target whose name says
it only writes. Rename the target and keep the alias, at the top of E5.

### D-wide — do not add a fourth index site

Section 6 lists three. Nothing new computes a mixed-radix index, and the comment at
`ShaderLibraryEmitter.cpp:376` is still in place. It is an accurate warning and Phase E deletes it.

### D5b — the diagnostic sink is here, and phase E is what makes it pay

Done. A diagnostic is a record with a severity, a code, a file, a range, and a message, and a sink
decides what becomes of it. `docs/phase-d-stage-separation-plan.md` §4c states the rest.

**Phase E owes that sink a source location.** `VerifyAxisNamesAreDeclared` searches the raw source
text for a string today, so the best it can report is an axis name. When each axis declaration moves
onto the `extern const static` as an attribute, the check becomes a walk of the declaration tree, and
Slang gives a location for each declaration through
`ISession::getDeclSourceLocation(DeclReflection*, SourceLocation*)`.

Two results follow, and both are phase E work:

- The string search goes away. It does not gain a location. A search that reads source text cannot
  tell a declaration from a comment that quotes one.
- An axis name that matches nothing stops being a message on a terminal and becomes a mark on the
  line that holds the mistake. That failure leaves the shader on its default value and reports
  nowhere else, so it is the failure that most deserves a location.

Walk the declaration tree once, here. Phase D must not attach a location to the parameter list path,
because that builds the weaker form first.

### D6 — test the evaluator through its interface

Section 4 adds two grammar levels. Write the stage 4 tests against `EvaluateSizeExpression` and the
resolve entry point, never against a parse tree, so the new levels do not churn the tests.

### Small, and only if the code is already open

Do not harden `PermutationValue` against new alternatives. Phase E adds `Enum` and `Type`. Reading
`ModulePolicy` through `FindPolicyForModule` rather than the static table directly also helps, so Phase
E swaps the source and not the call sites.

---

## 10. Steps

E1 to E4 change **how**, and every one is verifiable byte for byte against a Phase D golden dump. E5 to
E7 change **what**, and each one adds capability that no golden file can cover.

| Step | Work | Verified by | Risk |
|---|---|---|---|
| E0a | The entry point scope walk, from §1c. **Done on 2026-08-21.** | **A probe module cooks and the cross-check agrees** | low |
| E0b | The `ParameterBlock` sub-object walk, from §1d. **Done on 2026-08-21.** | **A probe module cooks and the cross-check agrees** | low |
| E0c | Reject a pointer type under a bound access model, from §1e | **A probe module fails the cook** | low |
| E0 | Slang interface spike, with citations | A written report | none |
| E1 | Comparison and logical levels in `SizeExpression` | Existing 28 tests, plus new ones | low |
| E2 | `AxisValueDomain`, `AxisKind`, `EarliestBindingTime`, `ActiveWhen`, `Require`, axis DAG, cycle check. `k_ModuleSpaces` still the source | The space dump, **once §9 D2 is settled** | medium |
| E3 | Depth-first enumeration with constraint propagation | **The variants dump is byte-identical** | medium |
| E4 | Sorted key table and binary search, replacing the storage index. Add the per-variant capability requirement to the manifest | Round trips, and the emitted tables shrink | **high** |
| E5 | Rename the JSON target, then the reader, the policy file, per-target sections, `CookValues`, `CookWhen` | Round trip against `JsonWriter` | medium |
| E6 | Axis attributes and the bootstrap compile. Delete `VerifyAxisNamesAreDeclared` | A cook of OceanFft with no registry entry | **high** |
| E7 | Interface axes, or the enum fallback from E0 | A new test shader | medium |
| E8 | Documents, and the measured numbers again | — | none |

E0a, E0b, and E0c come first. They are independent of everything else here, they touch the stage 3
code that E6 also touches, and the capability they restore is one an author will reach for long before
phase E lands. Take E0a first: it lifts the binding range walk out of `ExtractRawBindings`, and E0b
recurses through the same lifted function. E0c stands alone and can go in any order.

E4 is the one to be careful with. It changes the emitted C++, the manifest variant table, and the
cooker's own arithmetic at the same time. The round trips catch an error, and the stage dumps say
where.

**The manifest E4 edits is not the one this plan was written against.** Phase D step D8b replaced
`ManifestLayout` with three run tables and three payload tables, and `ManifestVariant` gained a
resource list index and a footprint list index. The `sizeof` assertions are gone: `k_IsManifestRecord`
now requires a record to be trivially copyable and to need no more than 8 byte alignment. Nothing
reads a manifest this build did not write, so the format is still cheap to change. Pin the sizes when
that stops being true.

**E6's acceptance test runs a path that was broken until 2026-08-20.** A module with no registered
space reached `space.front()` on an empty vector and aborted the cook, so "a cook of OceanFft with no
registry entry" would have failed for a reason that had nothing to do with E6.
`EnumerateActiveCombinations` is fixed and `PermutationIndexTest` now covers the empty space, but run
that cook once before E6 starts rather than at the end of it.

E6 is the one that pays for the phase. After it, an axis name cannot drift from its declaration, because
there is only one name.

---

## 11. What this leaves you with

An author declares an axis where the constant lives, and it propagates through every module that
includes the file.

A tech artist edits one checked-in file to decide what actually cooks, and never opens a C++ file.

A constraint that no tree can express is one line, and an assignment that breaks it never reaches the
compiler.

Four hundred variants cost 3.2 KiB of index instead of 4 MiB of holes.

And the failure mode that rule 7 of the handoff exists to catch cannot be written down.
