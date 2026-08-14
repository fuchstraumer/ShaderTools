#pragma once
#ifndef VELOX_SHADER_MANIFEST_HPP
#define VELOX_SHADER_MANIFEST_HPP
#include "shader/ShaderLibraryTypes.hpp"
#include <cstddef>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

/**
 * @brief A read-only view over one cooked shader module, stored as a flat byte span.
 *
 * The generated C++ library and this manifest hold the same data. The library compiles into the
 * program. The manifest arrives as bytes, so a running program can accept a new one.
 *
 * Every cross-reference in the file is a uint32 index, never a pointer. The reader is therefore a set
 * of spans over one byte span. It allocates nothing to open a file, and it relocates nothing.
 *
 * The byte span must outlive every view and every provider that reads it. Names and shader text point
 * into that span.
 */
namespace velox
{

inline constexpr uint32_t k_ShaderManifestMagic = 0x48535856u;
inline constexpr uint32_t k_ShaderManifestVersion = 1u;
/** A slot in the variant index table that no variant occupies. */
inline constexpr uint32_t k_ShaderManifestNoIndex = 0xFFFFFFFFu;

enum class ShaderManifestError : uint16_t
{
    Invalid = 0,
    Success = 1,
    TooSmall = 2,
    BadMagic = 3,
    VersionMismatch = 4,
    SizeMismatch = 5,
    SectionOutOfBounds = 6,
    IndexOutOfBounds = 7,
    /** The byte span does not start on an 8-byte boundary. The reader maps records in place, so it
     * cannot accept a span that would make a 64-bit field unaligned. */
    Misaligned = 8,
};

template<typename T>
using ManifestResult = std::expected<T, ShaderManifestError>;

std::string_view ToString(ShaderManifestError error) noexcept;

/** @brief Fixed header at offset zero. Each section is an offset from the start of the file and a
 * count of records. All values are little-endian. */
struct ShaderManifestHeader
{
    uint32_t Magic{ 0u };
    uint32_t Version{ 0u };
    uint32_t FileSize{ 0u };
    uint32_t ModuleNameString{ 0u };

    uint32_t StringTableOffset{ 0u };
    uint32_t StringCount{ 0u };
    uint32_t StringBlobOffset{ 0u };
    uint32_t StringBlobSize{ 0u };

    uint32_t SourceTableOffset{ 0u };
    uint32_t SourceCount{ 0u };
    uint32_t SourceBlobOffset{ 0u };
    uint32_t SourceBlobSize{ 0u };

    uint32_t BindingTableOffset{ 0u };
    uint32_t BindingCount{ 0u };
    uint32_t LayoutTableOffset{ 0u };
    uint32_t LayoutCount{ 0u };

    uint32_t EntryPointTableOffset{ 0u };
    uint32_t EntryPointCount{ 0u };
    uint32_t SlotTableOffset{ 0u };
    uint32_t SlotCount{ 0u };

    uint32_t VariantTableOffset{ 0u };
    uint32_t VariantCount{ 0u };
    uint32_t VariantIndexTableOffset{ 0u };
    uint32_t VariantIndexCount{ 0u };

    uint32_t AxisTableOffset{ 0u };
    uint32_t AxisCount{ 0u };
    uint32_t AxisValueTableOffset{ 0u };
    uint32_t AxisValueCount{ 0u };

    uint32_t RasterTableOffset{ 0u };
    uint32_t RasterCount{ 0u };
    uint32_t VertexInputTableOffset{ 0u };
    uint32_t VertexInputCount{ 0u };
    uint32_t ColorTargetTableOffset{ 0u };
    uint32_t ColorTargetCount{ 0u };
    uint32_t UniformMemberTableOffset{ 0u };
    uint32_t UniformMemberCount{ 0u };
};

struct ManifestStringRef
{
    uint32_t Offset{ 0u };
    uint32_t Length{ 0u };
};

struct ManifestSourceRef
{
    uint32_t Offset{ 0u };
    uint32_t Length{ 0u };
};

/** @brief One resource binding. Field order puts the 8-byte members first, so the record needs no
 * padding on any target and its size stays the same on every compiler. */
struct ManifestBinding
{
    uint64_t ByteSize{ 0u };
    uint64_t DerivedElementCount{ 0u };
    uint32_t NameString{ 0u };
    uint32_t Group{ 0u };
    uint32_t Binding{ 0u };
    uint32_t ElementStride{ 0u };
    uint32_t ArrayCount{ 1u };
    uint32_t DerivedExtentX{ 0u };
    uint32_t DerivedExtentY{ 0u };
    uint32_t DerivedExtentZ{ 0u };
    uint32_t StorageFormat{ 0u };
    uint32_t FirstUniformMember{ 0u };
    uint32_t UniformMemberCount{ 0u };
    uint32_t Reserved{ 0u };
    uint8_t Kind{ 0u };
    uint8_t Shape{ 0u };
    uint8_t SampleType{ 0u };
    uint8_t StorageAccess{ 0u };
    uint8_t SamplerType{ 0u };
    uint8_t Reserved0{ 0u };
    uint8_t Reserved1{ 0u };
    uint8_t Reserved2{ 0u };
};

/** @brief A run of bindings in the binding table. Two variants that share a layout name the same run. */
struct ManifestLayout
{
    uint32_t FirstBinding{ 0u };
    uint32_t BindingCount{ 0u };
};

struct ManifestEntryPoint
{
    uint32_t NameString{ 0u };
    uint32_t Stage{ 0u };
};

/** @brief What one entry point of one variant resolves to. */
struct ManifestSlot
{
    uint32_t SourceIndex{ 0u };
    uint32_t LayoutIndex{ 0u };
    uint32_t WorkgroupX{ 1u };
    uint32_t WorkgroupY{ 1u };
    uint32_t WorkgroupZ{ 1u };
    uint32_t RasterIndex{ 0u };
};

struct ManifestVertexInput
{
    uint32_t SemanticNameString{ 0u };
    uint32_t SemanticIndex{ 0u };
    uint32_t Location{ 0u };
    uint32_t ScalarType{ 0u };
    uint32_t ComponentCount{ 0u };
    uint32_t Reserved{ 0u };
};

struct ManifestUniformMember
{
    uint32_t NameString{ 0u };
    uint32_t Offset{ 0u };
    uint32_t Size{ 0u };
    uint32_t ArrayCount{ 1u };
};

struct ManifestColorTarget
{
    uint32_t Location{ 0u };
    uint32_t ScalarType{ 0u };
    uint32_t ComponentCount{ 0u };
    uint32_t Reserved{ 0u };
};

/** @brief Runs of vertex inputs and color targets. A compute entry point names a raster record whose
 * counts are both zero, so every slot can name one and no accessor needs a stage test. */
struct ManifestRaster
{
    uint32_t FirstVertexInput{ 0u };
    uint32_t VertexInputCount{ 0u };
    uint32_t FirstColorTarget{ 0u };
    uint32_t ColorTargetCount{ 0u };
    uint32_t WritesFragDepth{ 0u };
    uint32_t Reserved{ 0u };
};

struct ManifestVariant
{
    uint32_t Index{ 0u };
    uint32_t FirstSlot{ 0u };
    uint32_t SlotCount{ 0u };
    uint32_t SuffixString{ 0u };
};

struct ManifestAxis
{
    uint32_t NameString{ 0u };
    uint32_t FirstValue{ 0u };
    uint32_t ValueCount{ 0u };
    uint32_t Reserved{ 0u };
};

static_assert(sizeof(ShaderManifestHeader) == 144u, "manifest header size is part of the file format");
static_assert(sizeof(ManifestBinding) == 72u, "binding record size is part of the file format");
static_assert(sizeof(ManifestSlot) == 24u, "slot record size is part of the file format");
static_assert(sizeof(ManifestVariant) == 16u, "variant record size is part of the file format");
static_assert(sizeof(ManifestAxis) == 16u, "axis record size is part of the file format");

/**
 * @brief Spans over one manifest byte span, checked once when it opens.
 *
 * Open() checks the magic, the version, and that every section lies inside the file. After it returns
 * a view, no accessor can read outside the span, so the accessors stay branch-light.
 */
class ShaderManifestView final
{
public:
    ShaderManifestView() noexcept;

    static ManifestResult<ShaderManifestView> Open(std::span<const std::byte> bytes) noexcept;

    [[nodiscard]] std::string_view ModuleName() const noexcept;
    [[nodiscard]] std::string_view String(uint32_t string_index) const noexcept;
    [[nodiscard]] std::string_view Source(uint32_t source_index) const noexcept;

    [[nodiscard]] std::span<const ManifestBinding> Bindings() const noexcept;
    [[nodiscard]] std::span<const ManifestBinding> LayoutBindings(uint32_t layout_index) const noexcept;
    [[nodiscard]] std::span<const ManifestEntryPoint> EntryPoints() const noexcept;
    [[nodiscard]] std::span<const ManifestVariant> Variants() const noexcept;
    [[nodiscard]] std::span<const ManifestAxis> Axes() const noexcept;
    [[nodiscard]] std::span<const int64_t> AxisValues(uint32_t axis_index) const noexcept;

    [[nodiscard]] std::span<const ManifestVertexInput> VertexInputs(uint32_t raster_index) const noexcept;
    [[nodiscard]] std::span<const ManifestColorTarget> ColorTargets(uint32_t raster_index) const noexcept;
    [[nodiscard]] bool WritesFragDepth(uint32_t raster_index) const noexcept;

    [[nodiscard]] std::span<const ManifestUniformMember> UniformMembers(
        const ManifestBinding& binding) const noexcept;

    /** @brief The slot for one entry point of one variant, or nullptr when the pair does not exist.
     *
     * `entry_point` is the `EntryPointId` value, so it counts from one and zero is Invalid.
     * `variant_index` is the dense index, the same number the generated library uses. */
    [[nodiscard]] const ManifestSlot* FindSlot(uint16_t entry_point,
                                               uint32_t variant_index) const noexcept;

private:
    std::span<const std::byte> bytes;
    const ShaderManifestHeader* header{ nullptr };
    std::span<const ManifestStringRef> strings;
    std::span<const ManifestSourceRef> sources;
    std::span<const ManifestBinding> bindings;
    std::span<const ManifestLayout> layouts;
    std::span<const ManifestEntryPoint> entryPoints;
    std::span<const ManifestSlot> slots;
    std::span<const ManifestVariant> variants;
    std::span<const uint32_t> variantIndices;
    std::span<const ManifestAxis> axes;
    std::span<const int64_t> axisValues;
    std::span<const ManifestRaster> rasterStates;
    std::span<const ManifestVertexInput> vertexInputs;
    std::span<const ManifestColorTarget> colorTargets;
    std::span<const ManifestUniformMember> uniformMembers;
};

/**
 * @brief Serves shader sources out of a manifest instead of out of generated C++.
 *
 * This is the second implementation of ShaderSourceProvider, and the reason the interface exists. A
 * watch-and-serve cooker sends a new manifest, the caller builds a new provider, and Generation()
 * moves. Nothing in the rendergraph changes.
 *
 * The constructor converts the manifest binding records into BindingInfo once. That is the only
 * allocation, and it is needed because BindingInfo holds string views while the file holds indices.
 */
class ManifestShaderSourceProvider final : public ShaderSourceProvider
{
public:
    ManifestShaderSourceProvider(ShaderManifestView view, uint64_t generation) noexcept;
    ~ManifestShaderSourceProvider() override;

    [[nodiscard]] std::string_view Source(uint16_t entry_point,
                                          uint32_t variant_index) const noexcept override;
    [[nodiscard]] std::span<const BindingInfo> Bindings(uint16_t entry_point,
                                                        uint32_t variant_index) const noexcept override;
    [[nodiscard]] WorkgroupSize Workgroup(uint16_t entry_point,
                                          uint32_t variant_index) const noexcept override;
    [[nodiscard]] uint64_t Generation() const noexcept override;

    [[nodiscard]] const ShaderManifestView& View() const noexcept;

private:
    ShaderManifestView view;
    /** Built before bindingInfos and reserved to its final size, so the spans below stay valid. */
    std::vector<UniformMemberInfo> memberInfos;
    std::vector<BindingInfo> bindingInfos;
    uint64_t generation{ 0u };
};

} // namespace velox

#endif // !VELOX_SHADER_MANIFEST_HPP
