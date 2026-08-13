#pragma once
#ifndef VELOX_SHADER_COOKER_DATA_SCHEMA_HPP
#define VELOX_SHADER_COOKER_DATA_SCHEMA_HPP
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/** The schema the cooker extracts and the rendergraph eventually consumes. Deliberately free of any
 * Slang, WebGPU, or filesystem type: it is the contract between the offline tool and the runtime, and
 * anything that leaks a compiler type into it becomes a dependency the engine has to carry. */
namespace velox::cooker
{

enum class BindingKind : uint8_t
{
    Invalid = 0,
    UniformBuffer,
    StorageBuffer,
    ReadOnlyStorageBuffer,
    SampledTexture,
    StorageTexture,
    Sampler,
};

enum class ShaderStageKind : uint8_t
{
    Invalid = 0,
    Vertex,
    Fragment,
    Compute,
};

std::string_view ToString(BindingKind kind) noexcept;
std::string_view ToString(ShaderStageKind stage) noexcept;

struct WorkgroupSize
{
    uint32_t X{ 1u };
    uint32_t Y{ 1u };
    uint32_t Z{ 1u };
};

/** A size the shader author declared with a `[vx_*]` attribute, already evaluated for this variant.
 * `Expression` is kept for diagnostics: when two shaders disagree about a shared resource, the error
 * has to name what each of them actually wrote, not just the numbers they arrived at. */
struct DerivedSize
{
    std::string Expression;
    uint64_t ElementCount{ 0u };
    uint32_t ExtentX{ 0u };
    uint32_t ExtentY{ 0u };
    uint32_t ExtentZ{ 0u };
    bool HasElementCount{ false };
    bool HasExtent{ false };
};

struct ReflectedBinding
{
    std::string Name;
    uint32_t Group{ 0u };
    uint32_t Binding{ 0u };
    BindingKind Kind{ BindingKind::Invalid };
    uint32_t EntryPointUsageMask{ 0u };
    DerivedSize Derived;
};

struct EntryPointReflection
{
    std::string Name;
    ShaderStageKind Stage{ ShaderStageKind::Invalid };
    WorkgroupSize Workgroup;
    std::vector<ReflectedBinding> Bindings;
};

struct CompiledEntryPoint
{
    std::string Name;
    std::string VariantSuffix;
    std::string Code;
    EntryPointReflection Reflection;
};

struct CompiledVariant
{
    std::string VariantSuffix;
    std::string VariantDescription;
    /** Dense mixed-radix index over the canonical assignment. Stable across cooks, and the key the
     * rendergraph resolves a variant with. */
    uint32_t VariantIndex{ 0u };
    std::vector<CompiledEntryPoint> EntryPoints;
};

bool SameBindingLocation(const ReflectedBinding& lhs, const ReflectedBinding& rhs) noexcept;
void SortBindingsByLocation(std::span<ReflectedBinding> bindings) noexcept;
std::string DescribeBinding(const ReflectedBinding& binding);

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_DATA_SCHEMA_HPP
