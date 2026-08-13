#pragma once
#ifndef VELOX_SHADER_COOKER_DATA_SCHEMA_HPP
#define VELOX_SHADER_COOKER_DATA_SCHEMA_HPP
#include "shader/ShaderLibraryTypes.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/** The schema the cooker extracts and the rendergraph eventually consumes. It names no Slang, WebGPU,
 * or filesystem type. It is the contract between the offline tool and the runtime. A compiler type
 * that leaks into it becomes a dependency the engine must carry.
 *
 * The enums live in shader/ShaderLibraryTypes.hpp, because the graph reads the same vocabulary. This
 * schema adds only the parts that the offline tool needs: owning strings, and the raw expression text
 * that a diagnostic must quote. */
namespace velox::cooker
{

std::string_view ToString(BindingKind kind) noexcept;
std::string_view ToString(ShaderStageKind stage) noexcept;
std::string_view ToString(ResourceShape shape) noexcept;
std::string_view ToString(TextureSampleType sample_type) noexcept;

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

/** What the shader states about one resource. The CPU side never writes any of this.
 *
 * `ElementStride` is the size of one element of a structured buffer, in bytes. The graph multiplies
 * it by an element count. A caller therefore never writes a stride, and a mirrored CPU struct with
 * wrong padding cannot size a buffer.
 *
 * `ByteSize` is the total size of a uniform block. Reflection fully determines it, so a uniform block
 * must never take an explicit size from the caller.
 *
 * `StorageFormat` is set only for a storage texture, because the shader spells the format into the
 * binding type there. A sampled texture leaves it Invalid: the shader says it samples a float4, not
 * that the texture is Rgba16Float. That choice stays with the caller. */
struct ReflectedBinding
{
    std::string Name;
    uint32_t Group{ 0u };
    uint32_t Binding{ 0u };
    BindingKind Kind{ BindingKind::Invalid };
    uint32_t EntryPointUsageMask{ 0u };

    uint32_t ElementStride{ 0u };
    uint64_t ByteSize{ 0u };
    uint32_t ArrayCount{ 1u };

    ResourceShape Shape{ ResourceShape::Invalid };
    TextureSampleType SampleType{ TextureSampleType::Invalid };
    TextureFormat StorageFormat{ TextureFormat::Invalid };
    StorageTextureAccess StorageAccess{ StorageTextureAccess::Invalid };
    SamplerBindingType SamplerType{ SamplerBindingType::Invalid };

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
