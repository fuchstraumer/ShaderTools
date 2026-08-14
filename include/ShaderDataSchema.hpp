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

    friend bool operator==(const DerivedSize&, const DerivedSize&) = default;
};

/** One member of a uniform block, with the offset and the size the shader gave it.
 *
 * This table removes the hand-mirrored CPU struct. A struct on the CPU side whose padding is one float
 * wrong writes every field after that point to the wrong place, and nothing reports it: the buffer is
 * the right total size and the shader reads whatever is there. Compile() can compare offsetof against
 * this table and fail on the commit that introduces the drift.
 *
 * A nested struct flattens into dotted names, so `Light.Color` is one row and the tree does not
 * survive. The consumer checks offsets, and a tree would only make that harder. */
struct ReflectedUniformMember
{
    std::string Name;
    uint32_t Offset{ 0u };
    uint32_t Size{ 0u };
    uint32_t ArrayCount{ 1u };

    friend bool operator==(const ReflectedUniformMember&, const ReflectedUniformMember&) = default;
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
    /** Filled only for a uniform block. Every other binding kind leaves it empty. */
    std::vector<ReflectedUniformMember> UniformMembers;

    /** The interner compares whole layouts, so every field a consumer reads must take part. */
    friend bool operator==(const ReflectedBinding&, const ReflectedBinding&) = default;
};

std::string_view ToString(VertexScalarType scalar_type) noexcept;

/** One vertex shader input.
 *
 * WGSL keeps only `@location`. The semantic name and index exist in the Slang source and nowhere in
 * the emitted text, so this record is the only place they survive. A vertex buffer layout that a
 * caller builds by hand can therefore be checked against what the shader asked for. */
struct ReflectedVertexInput
{
    std::string SemanticName;
    uint32_t SemanticIndex{ 0u };
    uint32_t Location{ 0u };
    VertexScalarType ScalarType{ VertexScalarType::Invalid };
    uint32_t ComponentCount{ 0u };

    friend bool operator==(const ReflectedVertexInput&, const ReflectedVertexInput&) = default;
};

/** One fragment shader color target.
 *
 * The format is not here, and it cannot be. `float4 : SV_Target0` is equally Rgba8Unorm and
 * Rgba16Float. The shader states the component count and the scalar type only. The caller states the
 * format, and Compile() can check the two agree. */
struct ReflectedColorTarget
{
    uint32_t Location{ 0u };
    VertexScalarType ScalarType{ VertexScalarType::Invalid };
    uint32_t ComponentCount{ 0u };

    friend bool operator==(const ReflectedColorTarget&, const ReflectedColorTarget&) = default;
};

/** Everything the cooker can say about the raster stages of one entry point.
 *
 * Slang declares no pipeline render state, so blend, cull, front face, depth compare, and topology are
 * not here and never will be. Those stay in RenderState on the CPU side.
 *
 * One struct holds the whole raster payload on purpose. The engine's RenderTarget type lands in
 * parallel with this work, so the remap must stay one function body. */
struct ReflectedRasterState
{
    std::vector<ReflectedVertexInput> VertexInputs;
    std::vector<ReflectedColorTarget> ColorTargets;
    /** True when the fragment shader writes SV_Depth. This is not the same fact as depthWrite in
     * RenderState: a pass that writes depth from the shader while depthWrite is off is a real error,
     * and this is what makes that error findable. */
    bool WritesFragDepth{ false };

    friend bool operator==(const ReflectedRasterState&, const ReflectedRasterState&) = default;
};

struct EntryPointReflection
{
    std::string Name;
    ShaderStageKind Stage{ ShaderStageKind::Invalid };
    WorkgroupSize Workgroup;
    std::vector<ReflectedBinding> Bindings;
    ReflectedRasterState Raster;
};

std::string DescribeRasterState(const ReflectedRasterState& raster);

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
std::string DescribeUniformMembers(const ReflectedBinding& binding);

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_DATA_SCHEMA_HPP
