#pragma once
#ifndef LODESTONE_SHADER_COOKER_RAW_LIBRARY_HPP
#define LODESTONE_SHADER_COOKER_RAW_LIBRARY_HPP
#include "PermutationSpace.hpp"
#include "ShaderDataSchema.hpp"
#include "ShaderLibraryTypes.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/** What stage 3 produces: everything the compiler said, and no opinion about any of it.
 *
 * Stage 3 is the only stage that talks to Slang. It carries the `[vx_*]` argument strings without
 * understanding them, because evaluating one needs the axis values, and that is stage 4's job. A type
 * here that held an evaluated number could not tell "not resolved yet" from "the shader declared
 * nothing", so the numbers are absent instead of zero.
 *
 * This model names no Slang type, so stage 4 and everything after it never links against a compiler.
 *
 * A binding record here holds **placement** and the shape of the resource. It holds no visibility and
 * no footprint, because those have different keys and different lifetimes. See
 * `docs/phase-d-stage-separation-plan.md` §4b. */
namespace lodestone
{

/** Where a resource lives under the bound access model, which is the only model today.
 *
 * Group and binding are not fields of `RawBinding`, and that is deliberate. They are concepts of one
 * access model. A pointer-model target places a resource at a byte offset inside a struct, and an
 * indexed target places it at a heap index. Phase F adds those alternatives here, and every consumer
 * that already asks the variant which model it is looking at keeps working. See
 * `docs/phase-f-vocabulary.md` §4. */
struct BoundPlacement
{
    uint32_t Group{ 0u };
    uint32_t Binding{ 0u };

    friend bool operator==(const BoundPlacement&, const BoundPlacement&) = default;
};

/** `std::monostate` means the target reported no placement for this resource. It is not the same fact
 * as group 0 binding 0, and a struct with two integers could not say the difference. */
using RawPlacement = std::variant<std::monostate, BoundPlacement>;

/** Null when this resource is not placed by group and binding. */
const BoundPlacement* GetBoundPlacement(const RawPlacement& placement) noexcept;

enum class RawSizeAttributeKind : uint8_t
{
    Invalid = 0,
    ElementCount,
    Extent2d,
    Extent3d,
};

std::string_view ToString(RawSizeAttributeKind kind) noexcept;

/** How many arguments the attribute of this kind takes. */
uint32_t ArgumentCountOf(RawSizeAttributeKind kind) noexcept;

/** One `[vx_*]` annotation, exactly as the shader author wrote it.
 *
 * A size travels as a string because Slang folds an attribute integer argument at compile time, while
 * the permutation constants are `extern const static` and fold at link time. So the argument reaches
 * reflection untouched, and stage 4 does the arithmetic once for each variant. */
struct RawSizeAttribute
{
    /** Index into `RawVariant::GlobalBindings`. */
    uint32_t BindingIndex{ 0u };
    RawSizeAttributeKind Kind{ RawSizeAttributeKind::Invalid };
    std::vector<std::string> Arguments;
};

/** What a resource is, and where it lives. Not how much of it, and not who reads it.
 *
 * `ElementStride` and `ByteSize` come from the type layout rather than from an annotation, so they are
 * properties of the declared type and stay here. The footprint an author asked for with a `[vx_*]`
 * annotation is per variant, and it travels in `RawVariant::SizeAttributes`. */
struct RawBinding
{
    std::string Name;
    RawPlacement Placement;
    BindingKind Kind{ BindingKind::Invalid };

    uint32_t ElementStride{ 0u };
    uint64_t ByteSize{ 0u };
    uint32_t ArrayCount{ 1u };

    ResourceShape Shape{ ResourceShape::Invalid };
    TextureSampleType SampleType{ TextureSampleType::Invalid };
    TextureFormat StorageFormat{ TextureFormat::Invalid };
    StorageTextureAccess StorageAccess{ StorageTextureAccess::Invalid };
    SamplerBindingType SamplerType{ SamplerBindingType::Invalid };

    std::vector<ReflectedUniformMember> UniformMembers;
};

/** Orders two resources by placement, so two cooks of one input give one order. A resource with no
 * placement sorts after every placed one. */
bool RawPlacementLess(const RawPlacement& lhs, const RawPlacement& rhs) noexcept;

struct RawEntryPoint
{
    std::string Name;
    std::string VariantSuffix;
    ShaderStageKind Stage{ ShaderStageKind::Invalid };
    WorkgroupSize Workgroup;
    /** The text the target backend generated. Stage 3 does not read it. */
    std::string TargetText;
    /** Indices into `RawVariant::GlobalBindings`, ascending. This is visibility. */
    std::vector<uint32_t> UsedBindingIndices;
    ReflectedRasterState Raster;
};

struct RawVariant
{
    std::string VariantSuffix;
    std::string VariantDescription;
    uint32_t VariantIndex{ 0u };
    std::vector<RawBinding> GlobalBindings;
    /** Unevaluated. Stage 4 turns these into numbers. */
    std::vector<RawSizeAttribute> SizeAttributes;
    std::vector<RawEntryPoint> EntryPoints;
};

/** Stage 3's per-module output.
 *
 * `ExternDefaults` is easy to miss and stage 4 cannot work without it. A size expression may name an
 * `extern const static` constant that no axis drives, and only Slang knows what that constant
 * defaults to. Stage 4 must not call Slang, so the defaults travel here. */
struct RawModule
{
    std::string Name;
    std::vector<std::string> EntryPointNames;
    std::vector<ExternConstantDefault> ExternDefaults;
    std::vector<RawVariant> Variants;
};

struct RawLibrary
{
    std::vector<RawModule> Modules;
};

} // namespace lodestone

#endif // !LODESTONE_SHADER_COOKER_RAW_LIBRARY_HPP
