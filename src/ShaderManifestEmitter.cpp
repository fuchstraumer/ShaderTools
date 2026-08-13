#include "ShaderManifestEmitter.hpp"
#include "shader/ShaderManifest.hpp"
#include <cstring>
#include <format>
#include <print>
#include <unordered_map>

namespace velox::cooker
{

namespace
{

    /** Collects strings once and hands back the index of each. Two equal strings share one entry, so
     * the blob holds each binding name a single time however many layouts name it. */
    class StringTableBuilder final
    {
    public:
        uint32_t Add(std::string_view text)
        {
            const std::unordered_map<std::string, uint32_t>::iterator found =
                lookup.find(std::string{ text });
            if (found != lookup.end())
            {
                return found->second;
            }

            const uint32_t index = static_cast<uint32_t>(references.size());
            references.push_back(
                ManifestStringRef{ static_cast<uint32_t>(blob.size()), static_cast<uint32_t>(text.size()) });
            blob.append(text);
            lookup.emplace(std::string{ text }, index);
            return index;
        }

        const std::vector<ManifestStringRef>& References() const noexcept
        {
            return references;
        }

        const std::string& Blob() const noexcept
        {
            return blob;
        }

    private:
        std::unordered_map<std::string, uint32_t> lookup;
        std::vector<ManifestStringRef> references;
        std::string blob;
    };

    void AppendBytes(std::string& out, const void* data, size_t size)
    {
        out.append(static_cast<const char*>(data), size);
    }

    /** Pads to the next 8-byte boundary. Binding records and axis values hold 64-bit fields, and the
     * reader maps them in place, so every section must start aligned. */
    void AlignTo8(std::string& out)
    {
        while ((out.size() % 8u) != 0u)
        {
            out.push_back('\0');
        }
    }

    template<typename RecordType>
    uint32_t AppendTable(std::string& out, const std::vector<RecordType>& records)
    {
        AlignTo8(out);
        const uint32_t offset = static_cast<uint32_t>(out.size());
        if (!records.empty())
        {
            AppendBytes(out, records.data(), records.size() * sizeof(RecordType));
        }

        return offset;
    }

    ManifestBinding MakeBindingRecord(const ReflectedBinding& binding,
                                      StringTableBuilder& strings,
                                      std::vector<ManifestUniformMember>& member_records)
    {
        ManifestBinding record;
        record.ByteSize = binding.ByteSize;
        record.DerivedElementCount = binding.Derived.HasElementCount ? binding.Derived.ElementCount : 0u;
        record.NameString = strings.Add(binding.Name);
        record.Group = binding.Group;
        record.Binding = binding.Binding;
        record.ElementStride = binding.ElementStride;
        record.ArrayCount = binding.ArrayCount;
        record.DerivedExtentX = binding.Derived.HasExtent ? binding.Derived.ExtentX : 0u;
        record.DerivedExtentY = binding.Derived.HasExtent ? binding.Derived.ExtentY : 0u;
        record.DerivedExtentZ = binding.Derived.HasExtent ? binding.Derived.ExtentZ : 0u;
        record.StorageFormat = static_cast<uint32_t>(binding.StorageFormat);
        record.Kind = static_cast<uint8_t>(binding.Kind);
        record.Shape = static_cast<uint8_t>(binding.Shape);
        record.SampleType = static_cast<uint8_t>(binding.SampleType);
        record.StorageAccess = static_cast<uint8_t>(binding.StorageAccess);
        record.SamplerType = static_cast<uint8_t>(binding.SamplerType);

        record.FirstUniformMember = static_cast<uint32_t>(member_records.size());
        record.UniformMemberCount = static_cast<uint32_t>(binding.UniformMembers.size());
        for (const ReflectedUniformMember& member : binding.UniformMembers)
        {
            ManifestUniformMember memberRecord;
            memberRecord.NameString = strings.Add(member.Name);
            memberRecord.Offset = member.Offset;
            memberRecord.Size = member.Size;
            memberRecord.ArrayCount = member.ArrayCount;
            member_records.push_back(memberRecord);
        }

        return record;
    }

    bool RecordMatchesBinding(const ManifestBinding& record, const ReflectedBinding& binding) noexcept
    {
        const uint64_t expectedElements = binding.Derived.HasElementCount ? binding.Derived.ElementCount : 0u;
        const uint32_t expectedExtentX = binding.Derived.HasExtent ? binding.Derived.ExtentX : 0u;
        const uint32_t expectedExtentY = binding.Derived.HasExtent ? binding.Derived.ExtentY : 0u;
        const uint32_t expectedExtentZ = binding.Derived.HasExtent ? binding.Derived.ExtentZ : 0u;

        return record.ByteSize == binding.ByteSize && record.DerivedElementCount == expectedElements &&
               record.Group == binding.Group && record.Binding == binding.Binding &&
               record.ElementStride == binding.ElementStride && record.ArrayCount == binding.ArrayCount &&
               record.DerivedExtentX == expectedExtentX && record.DerivedExtentY == expectedExtentY &&
               record.DerivedExtentZ == expectedExtentZ &&
               record.StorageFormat == static_cast<uint32_t>(binding.StorageFormat) &&
               record.Kind == static_cast<uint8_t>(binding.Kind) &&
               record.Shape == static_cast<uint8_t>(binding.Shape) &&
               record.SampleType == static_cast<uint8_t>(binding.SampleType) &&
               record.StorageAccess == static_cast<uint8_t>(binding.StorageAccess) &&
               record.SamplerType == static_cast<uint8_t>(binding.SamplerType);
    }

} // namespace

std::string MakeManifestFileName(std::string_view module_name)
{
    return std::format("{}.vxshaders", module_name);
}

std::string EmitShaderManifest(const CookedModule& module)
{
    StringTableBuilder strings;

    const uint32_t moduleNameString = strings.Add(module.Name);

    std::vector<ManifestEntryPoint> entryPointRecords;
    entryPointRecords.reserve(module.EntryPoints.size());
    for (const LibraryEntryPoint& entryPoint : module.EntryPoints)
    {
        entryPointRecords.push_back(ManifestEntryPoint{ strings.Add(entryPoint.Name),
                                                        static_cast<uint32_t>(entryPoint.Stage) });
    }

    std::vector<ManifestUniformMember> uniformMemberRecords;
    std::vector<ManifestBinding> bindingRecords;
    std::vector<ManifestLayout> layoutRecords;
    layoutRecords.reserve(module.Layouts.size());
    for (const ShaderLayout& layout : module.Layouts)
    {
        layoutRecords.push_back(ManifestLayout{ static_cast<uint32_t>(bindingRecords.size()),
                                                static_cast<uint32_t>(layout.size()) });
        for (const ReflectedBinding& binding : layout)
        {
            bindingRecords.push_back(MakeBindingRecord(binding, strings, uniformMemberRecords));
        }
    }

    std::vector<ManifestVertexInput> vertexInputRecords;
    std::vector<ManifestColorTarget> colorTargetRecords;
    std::vector<ManifestRaster> rasterRecords;
    rasterRecords.reserve(module.RasterStates.size());
    for (const ReflectedRasterState& raster : module.RasterStates)
    {
        ManifestRaster record;
        record.FirstVertexInput = static_cast<uint32_t>(vertexInputRecords.size());
        record.VertexInputCount = static_cast<uint32_t>(raster.VertexInputs.size());
        record.FirstColorTarget = static_cast<uint32_t>(colorTargetRecords.size());
        record.ColorTargetCount = static_cast<uint32_t>(raster.ColorTargets.size());
        record.WritesFragDepth = raster.WritesFragDepth ? 1u : 0u;
        rasterRecords.push_back(record);

        for (const ReflectedVertexInput& input : raster.VertexInputs)
        {
            ManifestVertexInput inputRecord;
            inputRecord.SemanticNameString = strings.Add(input.SemanticName);
            inputRecord.SemanticIndex = input.SemanticIndex;
            inputRecord.Location = input.Location;
            inputRecord.ScalarType = static_cast<uint32_t>(input.ScalarType);
            inputRecord.ComponentCount = input.ComponentCount;
            vertexInputRecords.push_back(inputRecord);
        }

        for (const ReflectedColorTarget& target : raster.ColorTargets)
        {
            ManifestColorTarget targetRecord;
            targetRecord.Location = target.Location;
            targetRecord.ScalarType = static_cast<uint32_t>(target.ScalarType);
            targetRecord.ComponentCount = target.ComponentCount;
            colorTargetRecords.push_back(targetRecord);
        }
    }

    std::vector<ManifestSlot> slotRecords;
    std::vector<ManifestVariant> variantRecords;
    variantRecords.reserve(module.Variants.size());
    for (const LibraryVariant& variant : module.Variants)
    {
        ManifestVariant record;
        record.Index = variant.Index;
        record.FirstSlot = static_cast<uint32_t>(slotRecords.size());
        record.SlotCount = static_cast<uint32_t>(module.EntryPoints.size());
        record.SuffixString = strings.Add(variant.Suffix);
        variantRecords.push_back(record);

        for (size_t i = 0u; i < module.EntryPoints.size(); ++i)
        {
            ManifestSlot slot;
            slot.SourceIndex = variant.SourceIndices[i];
            slot.LayoutIndex = variant.LayoutIndices[i];
            slot.WorkgroupX = variant.Workgroups[i].X;
            slot.WorkgroupY = variant.Workgroups[i].Y;
            slot.WorkgroupZ = variant.Workgroups[i].Z;
            slot.RasterIndex = variant.RasterIndices[i];
            slotRecords.push_back(slot);
        }
    }

    std::vector<uint32_t> variantIndexRecords(module.SpaceSize, k_ShaderManifestNoIndex);
    for (size_t i = 0u; i < module.Variants.size(); ++i)
    {
        const uint32_t denseIndex = module.Variants[i].Index;
        if (denseIndex < variantIndexRecords.size())
        {
            variantIndexRecords[denseIndex] = static_cast<uint32_t>(i);
        }
    }

    std::vector<ManifestAxis> axisRecords;
    std::vector<int64_t> axisValueRecords;
    if (module.Space != nullptr)
    {
        axisRecords.reserve(module.Space->size());
        for (const PermutationAxis* axis : *module.Space)
        {
            ManifestAxis record;
            record.NameString = strings.Add(axis->Name);
            record.FirstValue = static_cast<uint32_t>(axisValueRecords.size());
            record.ValueCount = static_cast<uint32_t>(axis->Values.size());
            axisRecords.push_back(record);

            for (const PermutationValue& value : axis->Values)
            {
                axisValueRecords.push_back(PermutationValueToInt64(value));
            }
        }
    }

    std::string sourceBlob;
    std::vector<ManifestSourceRef> sourceRecords;
    sourceRecords.reserve(module.Sources.size());
    for (const std::string& source : module.Sources)
    {
        sourceRecords.push_back(ManifestSourceRef{ static_cast<uint32_t>(sourceBlob.size()),
                                                   static_cast<uint32_t>(source.size()) });
        sourceBlob.append(source);
    }

    ShaderManifestHeader header;
    header.Magic = k_ShaderManifestMagic;
    header.Version = k_ShaderManifestVersion;
    header.ModuleNameString = moduleNameString;

    std::string bytes;
    bytes.reserve(sourceBlob.size() + strings.Blob().size() + (1u << 16));
    bytes.resize(sizeof(ShaderManifestHeader), '\0');

    header.StringTableOffset = AppendTable(bytes, strings.References());
    header.StringCount = static_cast<uint32_t>(strings.References().size());

    AlignTo8(bytes);
    header.StringBlobOffset = static_cast<uint32_t>(bytes.size());
    header.StringBlobSize = static_cast<uint32_t>(strings.Blob().size());
    bytes.append(strings.Blob());

    header.SourceTableOffset = AppendTable(bytes, sourceRecords);
    header.SourceCount = static_cast<uint32_t>(sourceRecords.size());

    AlignTo8(bytes);
    header.SourceBlobOffset = static_cast<uint32_t>(bytes.size());
    header.SourceBlobSize = static_cast<uint32_t>(sourceBlob.size());
    bytes.append(sourceBlob);

    header.BindingTableOffset = AppendTable(bytes, bindingRecords);
    header.BindingCount = static_cast<uint32_t>(bindingRecords.size());
    header.LayoutTableOffset = AppendTable(bytes, layoutRecords);
    header.LayoutCount = static_cast<uint32_t>(layoutRecords.size());
    header.EntryPointTableOffset = AppendTable(bytes, entryPointRecords);
    header.EntryPointCount = static_cast<uint32_t>(entryPointRecords.size());
    header.SlotTableOffset = AppendTable(bytes, slotRecords);
    header.SlotCount = static_cast<uint32_t>(slotRecords.size());
    header.VariantTableOffset = AppendTable(bytes, variantRecords);
    header.VariantCount = static_cast<uint32_t>(variantRecords.size());
    header.VariantIndexTableOffset = AppendTable(bytes, variantIndexRecords);
    header.VariantIndexCount = static_cast<uint32_t>(variantIndexRecords.size());
    header.AxisTableOffset = AppendTable(bytes, axisRecords);
    header.AxisCount = static_cast<uint32_t>(axisRecords.size());
    header.AxisValueTableOffset = AppendTable(bytes, axisValueRecords);
    header.AxisValueCount = static_cast<uint32_t>(axisValueRecords.size());
    header.RasterTableOffset = AppendTable(bytes, rasterRecords);
    header.RasterCount = static_cast<uint32_t>(rasterRecords.size());
    header.VertexInputTableOffset = AppendTable(bytes, vertexInputRecords);
    header.VertexInputCount = static_cast<uint32_t>(vertexInputRecords.size());
    header.ColorTargetTableOffset = AppendTable(bytes, colorTargetRecords);
    header.ColorTargetCount = static_cast<uint32_t>(colorTargetRecords.size());
    header.UniformMemberTableOffset = AppendTable(bytes, uniformMemberRecords);
    header.UniformMemberCount = static_cast<uint32_t>(uniformMemberRecords.size());

    AlignTo8(bytes);
    header.FileSize = static_cast<uint32_t>(bytes.size());

    std::memcpy(bytes.data(), &header, sizeof(ShaderManifestHeader));

    return bytes;
}

CookResult<void> VerifyManifestRoundTrip(const CookedModule& module, const std::string& manifest_bytes)
{
    const std::span<const std::byte> raw{ reinterpret_cast<const std::byte*>(manifest_bytes.data()),
                                          manifest_bytes.size() };

    const ManifestResult<ShaderManifestView> opened = ShaderManifestView::Open(raw);
    if (!opened.has_value())
    {
        std::println(stderr,
                     "[shader_cooker] module {} manifest does not open: {}",
                     module.Name,
                     ToString(opened.error()));
        return std::unexpected(CookError::LibraryRoundTripFailed);
    }

    const ShaderManifestView& view = opened.value();

    if (view.ModuleName() != module.Name)
    {
        std::println(stderr,
                     "[shader_cooker] manifest names module '{}', but the cook produced '{}'",
                     view.ModuleName(),
                     module.Name);
        return std::unexpected(CookError::LibraryRoundTripFailed);
    }

    const ManifestShaderSourceProvider provider{ view, 0u };
    uint32_t checked = 0u;

    for (const LibraryVariant& variant : module.Variants)
    {
        for (size_t i = 0u; i < module.EntryPoints.size(); ++i)
        {
            // The provider takes the EntryPointId value, which counts from one.
            const uint16_t entryPoint = static_cast<uint16_t>(i + 1u);

            const std::string_view expectedSource = ResolveSource(module, variant, i);
            if (provider.Source(entryPoint, variant.Index) != expectedSource)
            {
                std::println(stderr,
                             "[shader_cooker] manifest returns different text for {} variant {} [{}]",
                             module.EntryPoints[i].Name,
                             variant.Index,
                             variant.Description);
                return std::unexpected(CookError::LibraryRoundTripFailed);
            }

            const WorkgroupSize expectedWorkgroup = variant.Workgroups[i];
            const WorkgroupSize readWorkgroup = provider.Workgroup(entryPoint, variant.Index);
            if (readWorkgroup.X != expectedWorkgroup.X || readWorkgroup.Y != expectedWorkgroup.Y ||
                readWorkgroup.Z != expectedWorkgroup.Z)
            {
                std::println(stderr,
                             "[shader_cooker] manifest returns a different workgroup size for {} variant {}",
                             module.EntryPoints[i].Name,
                             variant.Index);
                return std::unexpected(CookError::LibraryRoundTripFailed);
            }

            const ShaderLayout& expectedLayout = module.Layouts[variant.LayoutIndices[i]];
            const std::span<const ManifestBinding> readBindings =
                view.LayoutBindings(variant.LayoutIndices[i]);

            if (readBindings.size() != expectedLayout.size())
            {
                std::println(stderr,
                             "[shader_cooker] manifest layout {} holds {} bindings, the cook produced {}",
                             variant.LayoutIndices[i],
                             readBindings.size(),
                             expectedLayout.size());
                return std::unexpected(CookError::LibraryRoundTripFailed);
            }

            for (size_t bindingIndex = 0u; bindingIndex < expectedLayout.size(); ++bindingIndex)
            {
                const ReflectedBinding& expected = expectedLayout[bindingIndex];
                const ManifestBinding& read = readBindings[bindingIndex];

                const std::span<const ManifestUniformMember> readMembers = view.UniformMembers(read);
                bool membersMatch = readMembers.size() == expected.UniformMembers.size();
                for (size_t memberIndex = 0u; membersMatch && memberIndex < readMembers.size();
                     ++memberIndex)
                {
                    const ReflectedUniformMember& expectedMember = expected.UniformMembers[memberIndex];
                    const ManifestUniformMember& readMember = readMembers[memberIndex];

                    membersMatch = view.String(readMember.NameString) == expectedMember.Name &&
                                   readMember.Offset == expectedMember.Offset &&
                                   readMember.Size == expectedMember.Size &&
                                   readMember.ArrayCount == expectedMember.ArrayCount;
                }

                if (view.String(read.NameString) != expected.Name ||
                    !RecordMatchesBinding(read, expected) || !membersMatch)
                {
                    std::println(stderr,
                                 "[shader_cooker] manifest binding '{}' of layout {} does not match the "
                                 "cook",
                                 expected.Name,
                                 variant.LayoutIndices[i]);
                    return std::unexpected(CookError::LibraryRoundTripFailed);
                }
            }

            const ReflectedRasterState& expectedRaster = module.RasterStates[variant.RasterIndices[i]];
            const uint32_t rasterIndex = variant.RasterIndices[i];

            const std::span<const ManifestVertexInput> readInputs = view.VertexInputs(rasterIndex);
            const std::span<const ManifestColorTarget> readTargets = view.ColorTargets(rasterIndex);

            if (readInputs.size() != expectedRaster.VertexInputs.size() ||
                readTargets.size() != expectedRaster.ColorTargets.size() ||
                view.WritesFragDepth(rasterIndex) != expectedRaster.WritesFragDepth)
            {
                std::println(stderr,
                             "[shader_cooker] manifest raster state {} does not match the cook for {}",
                             rasterIndex,
                             module.EntryPoints[i].Name);
                return std::unexpected(CookError::LibraryRoundTripFailed);
            }

            for (size_t inputIndex = 0u; inputIndex < readInputs.size(); ++inputIndex)
            {
                const ReflectedVertexInput& expectedInput = expectedRaster.VertexInputs[inputIndex];
                const ManifestVertexInput& readInput = readInputs[inputIndex];

                if (view.String(readInput.SemanticNameString) != expectedInput.SemanticName ||
                    readInput.SemanticIndex != expectedInput.SemanticIndex ||
                    readInput.Location != expectedInput.Location ||
                    readInput.ScalarType != static_cast<uint32_t>(expectedInput.ScalarType) ||
                    readInput.ComponentCount != expectedInput.ComponentCount)
                {
                    std::println(stderr,
                                 "[shader_cooker] manifest vertex input '{}' does not match the cook",
                                 expectedInput.SemanticName);
                    return std::unexpected(CookError::LibraryRoundTripFailed);
                }
            }

            for (size_t targetIndex = 0u; targetIndex < readTargets.size(); ++targetIndex)
            {
                const ReflectedColorTarget& expectedTarget = expectedRaster.ColorTargets[targetIndex];
                const ManifestColorTarget& readTarget = readTargets[targetIndex];

                if (readTarget.Location != expectedTarget.Location ||
                    readTarget.ScalarType != static_cast<uint32_t>(expectedTarget.ScalarType) ||
                    readTarget.ComponentCount != expectedTarget.ComponentCount)
                {
                    std::println(stderr,
                                 "[shader_cooker] manifest color target {} does not match the cook",
                                 expectedTarget.Location);
                    return std::unexpected(CookError::LibraryRoundTripFailed);
                }
            }

            ++checked;
        }
    }

    std::println(stderr,
                 "[shader_cooker] module {} manifest round trip verified: {} entrypoint variants read back "
                 "identical ({} KiB)",
                 module.Name,
                 checked,
                 manifest_bytes.size() / 1024u);

    return {};
}

} // namespace velox::cooker
