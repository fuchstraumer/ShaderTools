#include "shader/ShaderManifest.hpp"
#include <cstring>
#include <magic_enum/magic_enum.hpp>

namespace velox
{

namespace
{

    /** True when a table of `count` records of `record_size` bytes starts at `offset` and stays
     * inside a file of `file_size` bytes. An empty table at any offset is in bounds. */
    bool TableIsInBounds(uint32_t offset, uint32_t count, size_t record_size, size_t file_size) noexcept
    {
        if (count == 0u)
        {
            return true;
        }

        const size_t span = static_cast<size_t>(count) * record_size;
        return offset <= file_size && span <= file_size - offset;
    }

    bool BlobIsInBounds(uint32_t offset, uint32_t size, size_t file_size) noexcept
    {
        if (size == 0u)
        {
            return true;
        }

        return offset <= file_size && size <= file_size - offset;
    }

    template<typename RecordType>
    std::span<const RecordType> MakeTable(std::span<const std::byte> bytes,
                                          uint32_t offset,
                                          uint32_t count) noexcept
    {
        if (count == 0u)
        {
            return {};
        }

        return std::span<const RecordType>{ reinterpret_cast<const RecordType*>(bytes.data() + offset),
                                            count };
    }

} // namespace

std::string_view ToString(ShaderManifestError error) noexcept
{
    return magic_enum::enum_name(error);
}

ShaderManifestView::ShaderManifestView() noexcept = default;

ManifestResult<ShaderManifestView> ShaderManifestView::Open(std::span<const std::byte> bytes) noexcept
{
    if (bytes.size() < sizeof(ShaderManifestHeader))
    {
        return std::unexpected(ShaderManifestError::TooSmall);
    }

    if ((reinterpret_cast<uintptr_t>(bytes.data()) % 8u) != 0u)
    {
        return std::unexpected(ShaderManifestError::Misaligned);
    }

    ShaderManifestHeader parsed{};
    std::memcpy(&parsed, bytes.data(), sizeof(ShaderManifestHeader));

    if (parsed.Magic != k_ShaderManifestMagic)
    {
        return std::unexpected(ShaderManifestError::BadMagic);
    }

    if (parsed.Version != k_ShaderManifestVersion)
    {
        return std::unexpected(ShaderManifestError::VersionMismatch);
    }

    if (parsed.FileSize != bytes.size())
    {
        return std::unexpected(ShaderManifestError::SizeMismatch);
    }

    const size_t fileSize = bytes.size();

    const bool sectionsFit =
        TableIsInBounds(parsed.StringTableOffset, parsed.StringCount, sizeof(ManifestStringRef), fileSize) &&
        BlobIsInBounds(parsed.StringBlobOffset, parsed.StringBlobSize, fileSize) &&
        TableIsInBounds(parsed.SourceTableOffset, parsed.SourceCount, sizeof(ManifestSourceRef), fileSize) &&
        BlobIsInBounds(parsed.SourceBlobOffset, parsed.SourceBlobSize, fileSize) &&
        TableIsInBounds(parsed.BindingTableOffset, parsed.BindingCount, sizeof(ManifestBinding), fileSize) &&
        TableIsInBounds(parsed.LayoutTableOffset, parsed.LayoutCount, sizeof(ManifestLayout), fileSize) &&
        TableIsInBounds(parsed.EntryPointTableOffset,
                        parsed.EntryPointCount,
                        sizeof(ManifestEntryPoint),
                        fileSize) &&
        TableIsInBounds(parsed.SlotTableOffset, parsed.SlotCount, sizeof(ManifestSlot), fileSize) &&
        TableIsInBounds(parsed.VariantTableOffset, parsed.VariantCount, sizeof(ManifestVariant), fileSize) &&
        TableIsInBounds(parsed.VariantIndexTableOffset,
                        parsed.VariantIndexCount,
                        sizeof(uint32_t),
                        fileSize) &&
        TableIsInBounds(parsed.AxisTableOffset, parsed.AxisCount, sizeof(ManifestAxis), fileSize) &&
        TableIsInBounds(parsed.AxisValueTableOffset, parsed.AxisValueCount, sizeof(int64_t), fileSize) &&
        TableIsInBounds(parsed.RasterTableOffset, parsed.RasterCount, sizeof(ManifestRaster), fileSize) &&
        TableIsInBounds(parsed.VertexInputTableOffset,
                        parsed.VertexInputCount,
                        sizeof(ManifestVertexInput),
                        fileSize) &&
        TableIsInBounds(parsed.ColorTargetTableOffset,
                        parsed.ColorTargetCount,
                        sizeof(ManifestColorTarget),
                        fileSize) &&
        TableIsInBounds(parsed.UniformMemberTableOffset,
                        parsed.UniformMemberCount,
                        sizeof(ManifestUniformMember),
                        fileSize);

    if (!sectionsFit)
    {
        return std::unexpected(ShaderManifestError::SectionOutOfBounds);
    }

    ShaderManifestView view;
    view.bytes = bytes;
    view.header = reinterpret_cast<const ShaderManifestHeader*>(bytes.data());
    view.strings = MakeTable<ManifestStringRef>(bytes, parsed.StringTableOffset, parsed.StringCount);
    view.sources = MakeTable<ManifestSourceRef>(bytes, parsed.SourceTableOffset, parsed.SourceCount);
    view.bindings = MakeTable<ManifestBinding>(bytes, parsed.BindingTableOffset, parsed.BindingCount);
    view.layouts = MakeTable<ManifestLayout>(bytes, parsed.LayoutTableOffset, parsed.LayoutCount);
    view.entryPoints =
        MakeTable<ManifestEntryPoint>(bytes, parsed.EntryPointTableOffset, parsed.EntryPointCount);
    view.slots = MakeTable<ManifestSlot>(bytes, parsed.SlotTableOffset, parsed.SlotCount);
    view.variants = MakeTable<ManifestVariant>(bytes, parsed.VariantTableOffset, parsed.VariantCount);
    view.variantIndices =
        MakeTable<uint32_t>(bytes, parsed.VariantIndexTableOffset, parsed.VariantIndexCount);
    view.axes = MakeTable<ManifestAxis>(bytes, parsed.AxisTableOffset, parsed.AxisCount);
    view.axisValues = MakeTable<int64_t>(bytes, parsed.AxisValueTableOffset, parsed.AxisValueCount);
    view.rasterStates = MakeTable<ManifestRaster>(bytes, parsed.RasterTableOffset, parsed.RasterCount);
    view.vertexInputs =
        MakeTable<ManifestVertexInput>(bytes, parsed.VertexInputTableOffset, parsed.VertexInputCount);
    view.colorTargets =
        MakeTable<ManifestColorTarget>(bytes, parsed.ColorTargetTableOffset, parsed.ColorTargetCount);
    view.uniformMembers = MakeTable<ManifestUniformMember>(bytes,
                                                           parsed.UniformMemberTableOffset,
                                                           parsed.UniformMemberCount);

    return view;
}

std::string_view ShaderManifestView::String(uint32_t string_index) const noexcept
{
    if (header == nullptr || string_index >= strings.size())
    {
        return {};
    }

    const ManifestStringRef& reference = strings[string_index];
    if (reference.Length > header->StringBlobSize ||
        reference.Offset > header->StringBlobSize - reference.Length)
    {
        return {};
    }

    const char* base = reinterpret_cast<const char*>(bytes.data() + header->StringBlobOffset);
    return std::string_view{ base + reference.Offset, reference.Length };
}

std::string_view ShaderManifestView::ModuleName() const noexcept
{
    if (header == nullptr)
    {
        return {};
    }

    return String(header->ModuleNameString);
}

std::string_view ShaderManifestView::Source(uint32_t source_index) const noexcept
{
    if (header == nullptr || source_index >= sources.size())
    {
        return {};
    }

    const ManifestSourceRef& reference = sources[source_index];
    if (reference.Length > header->SourceBlobSize ||
        reference.Offset > header->SourceBlobSize - reference.Length)
    {
        return {};
    }

    const char* base = reinterpret_cast<const char*>(bytes.data() + header->SourceBlobOffset);
    return std::string_view{ base + reference.Offset, reference.Length };
}

std::span<const ManifestBinding> ShaderManifestView::Bindings() const noexcept
{
    return bindings;
}

std::span<const ManifestBinding> ShaderManifestView::LayoutBindings(uint32_t layout_index) const noexcept
{
    if (layout_index >= layouts.size())
    {
        return {};
    }

    const ManifestLayout& layout = layouts[layout_index];
    if (layout.FirstBinding > bindings.size() ||
        layout.BindingCount > bindings.size() - layout.FirstBinding)
    {
        return {};
    }

    return bindings.subspan(layout.FirstBinding, layout.BindingCount);
}

std::span<const ManifestEntryPoint> ShaderManifestView::EntryPoints() const noexcept
{
    return entryPoints;
}

std::span<const ManifestVariant> ShaderManifestView::Variants() const noexcept
{
    return variants;
}

std::span<const ManifestAxis> ShaderManifestView::Axes() const noexcept
{
    return axes;
}

std::span<const int64_t> ShaderManifestView::AxisValues(uint32_t axis_index) const noexcept
{
    if (axis_index >= axes.size())
    {
        return {};
    }

    const ManifestAxis& axis = axes[axis_index];
    if (axis.FirstValue > axisValues.size() || axis.ValueCount > axisValues.size() - axis.FirstValue)
    {
        return {};
    }

    return axisValues.subspan(axis.FirstValue, axis.ValueCount);
}

std::span<const ManifestVertexInput> ShaderManifestView::VertexInputs(uint32_t raster_index) const noexcept
{
    if (raster_index >= rasterStates.size())
    {
        return {};
    }

    const ManifestRaster& raster = rasterStates[raster_index];
    if (raster.FirstVertexInput > vertexInputs.size() ||
        raster.VertexInputCount > vertexInputs.size() - raster.FirstVertexInput)
    {
        return {};
    }

    return vertexInputs.subspan(raster.FirstVertexInput, raster.VertexInputCount);
}

std::span<const ManifestColorTarget> ShaderManifestView::ColorTargets(uint32_t raster_index) const noexcept
{
    if (raster_index >= rasterStates.size())
    {
        return {};
    }

    const ManifestRaster& raster = rasterStates[raster_index];
    if (raster.FirstColorTarget > colorTargets.size() ||
        raster.ColorTargetCount > colorTargets.size() - raster.FirstColorTarget)
    {
        return {};
    }

    return colorTargets.subspan(raster.FirstColorTarget, raster.ColorTargetCount);
}

bool ShaderManifestView::WritesFragDepth(uint32_t raster_index) const noexcept
{
    if (raster_index >= rasterStates.size())
    {
        return false;
    }

    return rasterStates[raster_index].WritesFragDepth != 0u;
}

std::span<const ManifestUniformMember> ShaderManifestView::UniformMembers(
    const ManifestBinding& binding) const noexcept
{
    if (binding.FirstUniformMember > uniformMembers.size() ||
        binding.UniformMemberCount > uniformMembers.size() - binding.FirstUniformMember)
    {
        return {};
    }

    return uniformMembers.subspan(binding.FirstUniformMember, binding.UniformMemberCount);
}

const ManifestSlot* ShaderManifestView::FindSlot(uint16_t entry_point,
                                                 uint32_t variant_index) const noexcept
{
    if (entry_point == 0u || variant_index >= variantIndices.size())
    {
        return nullptr;
    }

    const uint32_t variantSlot = variantIndices[variant_index];
    if (variantSlot == k_ShaderManifestNoIndex || variantSlot >= variants.size())
    {
        return nullptr;
    }

    const uint32_t entryPointIndex = static_cast<uint32_t>(entry_point) - 1u;

    const ManifestVariant& variant = variants[variantSlot];
    if (entryPointIndex >= variant.SlotCount)
    {
        return nullptr;
    }

    const uint32_t slotIndex = variant.FirstSlot + entryPointIndex;
    if (slotIndex >= slots.size())
    {
        return nullptr;
    }

    return &slots[slotIndex];
}

ManifestShaderSourceProvider::ManifestShaderSourceProvider(ShaderManifestView _view,
                                                           uint64_t _generation) noexcept :
    view{ _view },
    generation{ _generation }
{
    const std::span<const ManifestBinding> records = view.Bindings();
    bindingInfos.reserve(records.size());

    size_t totalMembers = 0u;
    for (const ManifestBinding& record : records)
    {
        totalMembers += record.UniformMemberCount;
    }

    memberInfos.reserve(totalMembers);
    for (const ManifestBinding& record : records)
    {
        for (const ManifestUniformMember& member : view.UniformMembers(record))
        {
            UniformMemberInfo info;
            info.Name = view.String(member.NameString);
            info.Offset = member.Offset;
            info.Size = member.Size;
            info.ArrayCount = member.ArrayCount;
            memberInfos.push_back(info);
        }
    }

    size_t memberCursor = 0u;
    for (const ManifestBinding& record : records)
    {
        BindingInfo info;
        info.Name = view.String(record.NameString);
        info.Group = record.Group;
        info.Binding = record.Binding;
        info.Kind = static_cast<BindingKind>(record.Kind);
        info.ElementStride = record.ElementStride;
        info.ByteSize = record.ByteSize;
        info.ArrayCount = record.ArrayCount;
        info.Shape = static_cast<ResourceShape>(record.Shape);
        info.SampleType = static_cast<TextureSampleType>(record.SampleType);
        info.StorageFormat = static_cast<TextureFormat>(record.StorageFormat);
        info.StorageAccess = static_cast<StorageTextureAccess>(record.StorageAccess);
        info.SamplerType = static_cast<SamplerBindingType>(record.SamplerType);
        info.DerivedElementCount = record.DerivedElementCount;
        info.DerivedExtentX = record.DerivedExtentX;
        info.DerivedExtentY = record.DerivedExtentY;
        info.DerivedExtentZ = record.DerivedExtentZ;

        if (record.UniformMemberCount != 0u)
        {
            info.Members = std::span<const UniformMemberInfo>{ memberInfos.data() + memberCursor,
                                                               record.UniformMemberCount };
            memberCursor += record.UniformMemberCount;
        }

        bindingInfos.push_back(info);
    }
}

ManifestShaderSourceProvider::~ManifestShaderSourceProvider() = default;

std::string_view ManifestShaderSourceProvider::Source(uint16_t entry_point,
                                                      uint32_t variant_index) const noexcept
{
    const ManifestSlot* slot = view.FindSlot(entry_point, variant_index);
    if (slot == nullptr)
    {
        return {};
    }

    return view.Source(slot->SourceIndex);
}

std::span<const BindingInfo> ManifestShaderSourceProvider::Bindings(uint16_t entry_point,
                                                                    uint32_t variant_index) const noexcept
{
    const ManifestSlot* slot = view.FindSlot(entry_point, variant_index);
    if (slot == nullptr)
    {
        return {};
    }

    const std::span<const ManifestBinding> records = view.LayoutBindings(slot->LayoutIndex);
    if (records.empty())
    {
        return {};
    }

    const size_t first = static_cast<size_t>(records.data() - view.Bindings().data());
    return std::span<const BindingInfo>{ bindingInfos.data() + first, records.size() };
}

WorkgroupSize ManifestShaderSourceProvider::Workgroup(uint16_t entry_point,
                                                      uint32_t variant_index) const noexcept
{
    const ManifestSlot* slot = view.FindSlot(entry_point, variant_index);
    if (slot == nullptr)
    {
        return WorkgroupSize{};
    }

    return WorkgroupSize{ slot->WorkgroupX, slot->WorkgroupY, slot->WorkgroupZ };
}

uint64_t ManifestShaderSourceProvider::Generation() const noexcept
{
    return generation;
}

const ShaderManifestView& ManifestShaderSourceProvider::View() const noexcept
{
    return view;
}

} // namespace velox
