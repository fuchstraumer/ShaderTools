#include "StageDump.hpp"
#include "ContentHash.hpp"
#include "ContentInterner.hpp"
#include "CookedLibrary.hpp"
#include "CookerOptions.hpp"
#include "JsonWriter.hpp"
#include "PermutationSpace.hpp"
#include "ShaderDataSchema.hpp"
#include "ShaderLibraryTypes.hpp"

#include <magic_enum/magic_enum.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace lodestone
{

namespace
{

    constexpr std::string_view k_UnbalancedDocument = R"({"error":"UnbalancedContainers"})";

    std::string FinishDocument(JsonWriter& writer)
    {
        JsonResult<std::string> document = writer.Finish();
        if (!document)
        {
            return std::string{ k_UnbalancedDocument };
        }

        return document.value();
    }

    void WriteAxisValue(JsonWriter& writer, const PermutationValue& value)
    {
        writer.String(ValueToSlangLiteral(value));
    }

    void WriteAssignment(JsonWriter& writer, const PermutationAssignment& assignment)
    {
        writer.BeginArray();
        for (const PermutationBinding& binding : assignment)
        {
            writer.BeginObject();
            writer.KeyString("axis", binding.first != nullptr ? binding.first->Name : std::string{});
            writer.Key("value");
            WriteAxisValue(writer, binding.second);
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteAxis(JsonWriter& writer, const PermutationAxis& axis)
    {
        writer.BeginObject();
        writer.KeyString("name", axis.Name);

        writer.Key("values");
        writer.BeginArray();
        for (const PermutationValue& value : axis.Values)
        {
            WriteAxisValue(writer, value);
        }
        writer.EndArray();

        if (axis.Parent != nullptr)
        {
            writer.KeyString("parent", axis.Parent->Name);
            writer.Key("requiredParentValue");
            WriteAxisValue(writer, axis.RequiredParentValue);
        }
        else
        {
            writer.KeyNull("parent");
            writer.KeyNull("requiredParentValue");
        }

        writer.EndObject();
    }

    void WriteUniformMembers(JsonWriter& writer, const ReflectedBinding& binding)
    {
        writer.Key("uniformMembers");
        writer.BeginArray();
        for (const ReflectedUniformMember& member : binding.UniformMembers)
        {
            writer.BeginObject();
            writer.KeyString("name", member.Name);
            writer.KeyUInt("offset", member.Offset);
            writer.KeyUInt("size", member.Size);
            writer.KeyUInt("arrayCount", member.ArrayCount);
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteDerivedSize(JsonWriter& writer, const DerivedSize& derived)
    {
        writer.Key("derived");
        writer.BeginObject();
        writer.KeyString("expression", derived.Expression);
        writer.KeyBool("hasElementCount", derived.HasElementCount);
        writer.KeyUInt("elementCount", derived.ElementCount);
        writer.KeyBool("hasExtent", derived.HasExtent);
        writer.KeyUInt("extentX", derived.ExtentX);
        writer.KeyUInt("extentY", derived.ExtentY);
        writer.KeyUInt("extentZ", derived.ExtentZ);
        writer.EndObject();
    }

    void WriteBinding(JsonWriter& writer, const ReflectedBinding& binding)
    {
        writer.BeginObject();
        writer.KeyString("name", binding.Name);
        writer.KeyUInt("group", binding.Group);
        writer.KeyUInt("binding", binding.Binding);
        writer.KeyString("kind", magic_enum::enum_name(binding.Kind));
        writer.KeyUInt("entryPointUsageMask", binding.EntryPointUsageMask);
        writer.KeyUInt("elementStride", binding.ElementStride);
        writer.KeyUInt("byteSize", binding.ByteSize);
        writer.KeyUInt("arrayCount", binding.ArrayCount);
        writer.KeyString("shape", magic_enum::enum_name(binding.Shape));
        writer.KeyString("sampleType", magic_enum::enum_name(binding.SampleType));
        writer.KeyString("storageFormat", magic_enum::enum_name(binding.StorageFormat));
        writer.KeyString("storageAccess", magic_enum::enum_name(binding.StorageAccess));
        writer.KeyString("samplerType", magic_enum::enum_name(binding.SamplerType));
        WriteDerivedSize(writer, binding.Derived);
        WriteUniformMembers(writer, binding);
        writer.EndObject();
    }

    void WriteVertexInputs(JsonWriter& writer, const ReflectedRasterState& raster)
    {
        writer.Key("vertexInputs");
        writer.BeginArray();
        for (const ReflectedVertexInput& input : raster.VertexInputs)
        {
            writer.BeginObject();
            writer.KeyString("semanticName", input.SemanticName);
            writer.KeyUInt("semanticIndex", input.SemanticIndex);
            writer.KeyUInt("location", input.Location);
            writer.KeyString("scalarType", magic_enum::enum_name(input.ScalarType));
            writer.KeyUInt("componentCount", input.ComponentCount);
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteColorTargets(JsonWriter& writer, const ReflectedRasterState& raster)
    {
        writer.Key("colorTargets");
        writer.BeginArray();
        for (const ReflectedColorTarget& target : raster.ColorTargets)
        {
            writer.BeginObject();
            writer.KeyUInt("location", target.Location);
            writer.KeyString("scalarType", magic_enum::enum_name(target.ScalarType));
            writer.KeyUInt("componentCount", target.ComponentCount);
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteIndexArray(JsonWriter& writer, std::string_view key, const std::vector<uint32_t>& indices)
    {
        writer.Key(key);
        writer.BeginArray();
        for (uint32_t index : indices)
        {
            writer.UInt(index);
        }
        writer.EndArray();
    }

    void WriteEntryPointTable(JsonWriter& writer, std::span<const LibraryEntryPoint> entry_points)
    {
        writer.Key("entryPoints");
        writer.BeginArray();
        for (const LibraryEntryPoint& entryPoint : entry_points)
        {
            writer.BeginObject();
            writer.KeyString("name", entryPoint.Name);
            writer.KeyString("stage", magic_enum::enum_name(entryPoint.Stage));
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteSourceTable(JsonWriter& writer, const CookedModule& module)
    {
        writer.Key("sources");
        writer.BeginArray();
        for (size_t i = 0u; i < module.Sources.size(); ++i)
        {
            writer.BeginObject();
            writer.KeyUInt("index", i);
            writer.KeyUInt("byteLength", module.Sources[i].size());
            writer.KeyUInt("contentHash", HashSourcePayload(module.Sources[i]));
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteLayoutTable(JsonWriter& writer, const CookedModule& module)
    {
        writer.Key("layouts");
        writer.BeginArray();
        for (size_t i = 0u; i < module.Layouts.size(); ++i)
        {
            writer.BeginObject();
            writer.KeyUInt("index", i);
            writer.KeyUInt("contentHash", HashLayoutPayload(module.Layouts[i]));
            writer.Key("bindings");
            writer.BeginArray();
            for (const ReflectedBinding& binding : module.Layouts[i])
            {
                WriteBinding(writer, binding);
            }
            writer.EndArray();
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteRasterTable(JsonWriter& writer, const CookedModule& module)
    {
        writer.Key("rasterStates");
        writer.BeginArray();
        for (size_t i = 0u; i < module.RasterStates.size(); ++i)
        {
            writer.BeginObject();
            writer.KeyUInt("index", i);
            writer.KeyUInt("contentHash", HashRasterPayload(module.RasterStates[i]));
            WriteVertexInputs(writer, module.RasterStates[i]);
            WriteColorTargets(writer, module.RasterStates[i]);
            writer.KeyBool("writesFragDepth", module.RasterStates[i].WritesFragDepth);
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteWorkgroups(JsonWriter& writer, const LibraryVariant& variant)
    {
        writer.Key("workgroups");
        writer.BeginArray();
        for (const WorkgroupSize& workgroup : variant.Workgroups)
        {
            writer.BeginObject();
            writer.KeyUInt("x", workgroup.X);
            writer.KeyUInt("y", workgroup.Y);
            writer.KeyUInt("z", workgroup.Z);
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteVariantTable(JsonWriter& writer, std::span<const LibraryVariant> variants)
    {
        writer.Key("variants");
        writer.BeginArray();
        for (const LibraryVariant& variant : variants)
        {
            writer.BeginObject();
            writer.KeyUInt("index", variant.Index);
            writer.KeyString("suffix", variant.Suffix);
            writer.KeyString("description", variant.Description);
            writer.Key("canonical");
            WriteAssignment(writer, variant.Canonical);
            WriteIndexArray(writer, "sourceIndices", variant.SourceIndices);
            WriteIndexArray(writer, "layoutIndices", variant.LayoutIndices);
            WriteIndexArray(writer, "rasterIndices", variant.RasterIndices);
            WriteWorkgroups(writer, variant);
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteInterner(JsonWriter& writer,
                       std::string_view key,
                       std::string_view hash_name,
                       bool dedupe_enabled,
                       const InternerStatistics& statistics)
    {
        writer.Key(key);
        writer.BeginObject();
        writer.KeyString("hashName", hash_name);
        writer.KeyBool("dedupeEnabled", dedupe_enabled);
        writer.KeyUInt("artifactsSeen", statistics.ArtifactsSeen);
        writer.KeyUInt("uniqueEntries", statistics.UniqueEntries);
        writer.KeyUInt("hashCollisions", statistics.HashCollisions);
        writer.KeyUInt("byteComparisons", statistics.ByteComparisons);
        writer.EndObject();
    }

    void WriteInternerTable(JsonWriter& writer, const CookedModule& module)
    {
        writer.Key("interners");
        writer.BeginObject();
        WriteInterner(writer,
                      "sources",
                      module.SourceTable.HashName,
                      module.SourceTable.DedupeEnabled,
                      module.SourceTable.Interning);
        WriteInterner(writer,
                      "layouts",
                      module.LayoutTable.HashName,
                      module.LayoutTable.DedupeEnabled,
                      module.LayoutTable.Interning);
        WriteInterner(writer,
                      "rasterStates",
                      module.RasterTable.HashName,
                      module.RasterTable.DedupeEnabled,
                      module.RasterTable.Interning);
        writer.EndObject();
    }

    void WriteRawPlacement(JsonWriter& writer, const RawPlacement& placement)
    {
        writer.Key("placement");
        const BoundPlacement* bound = GetBoundPlacement(placement);
        if (bound == nullptr)
        {
            writer.Null();
            return;
        }

        writer.BeginObject();
        writer.KeyString("model", "Bound");
        writer.KeyUInt("group", bound->Group);
        writer.KeyUInt("binding", bound->Binding);
        writer.EndObject();
    }

    void WriteRawBindings(JsonWriter& writer, const RawVariant& variant)
    {
        writer.Key("globalBindings");
        writer.BeginArray();
        for (size_t i = 0u; i < variant.GlobalBindings.size(); ++i)
        {
            const RawBinding& binding = variant.GlobalBindings[i];
            writer.BeginObject();
            writer.KeyUInt("index", i);
            writer.KeyString("name", binding.Name);
            WriteRawPlacement(writer, binding.Placement);
            writer.KeyString("kind", magic_enum::enum_name(binding.Kind));
            writer.KeyUInt("elementStride", binding.ElementStride);
            writer.KeyUInt("byteSize", binding.ByteSize);
            writer.KeyUInt("arrayCount", binding.ArrayCount);
            writer.KeyString("shape", magic_enum::enum_name(binding.Shape));
            writer.KeyString("sampleType", magic_enum::enum_name(binding.SampleType));
            writer.KeyString("storageFormat", magic_enum::enum_name(binding.StorageFormat));
            writer.KeyString("storageAccess", magic_enum::enum_name(binding.StorageAccess));
            writer.KeyString("samplerType", magic_enum::enum_name(binding.SamplerType));

            writer.Key("uniformMembers");
            writer.BeginArray();
            for (const ReflectedUniformMember& member : binding.UniformMembers)
            {
                writer.BeginObject();
                writer.KeyString("name", member.Name);
                writer.KeyUInt("offset", member.Offset);
                writer.KeyUInt("size", member.Size);
                writer.KeyUInt("arrayCount", member.ArrayCount);
                writer.EndObject();
            }
            writer.EndArray();

            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteRawSizeAttributes(JsonWriter& writer, const RawVariant& variant)
    {
        writer.Key("sizeAttributes");
        writer.BeginArray();
        for (const RawSizeAttribute& attribute : variant.SizeAttributes)
        {
            writer.BeginObject();
            writer.KeyUInt("bindingIndex", attribute.BindingIndex);
            writer.KeyString("attribute", ToString(attribute.Kind));
            writer.Key("arguments");
            writer.BeginArray();
            for (const std::string& argument : attribute.Arguments)
            {
                writer.String(argument);
            }
            writer.EndArray();
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteRawEntryPoints(JsonWriter& writer, const RawVariant& variant)
    {
        writer.Key("entryPoints");
        writer.BeginArray();
        for (const RawEntryPoint& entryPoint : variant.EntryPoints)
        {
            writer.BeginObject();
            writer.KeyString("name", entryPoint.Name);
            writer.KeyString("stage", magic_enum::enum_name(entryPoint.Stage));
            writer.Key("workgroup");
            writer.BeginObject();
            writer.KeyUInt("x", entryPoint.Workgroup.X);
            writer.KeyUInt("y", entryPoint.Workgroup.Y);
            writer.KeyUInt("z", entryPoint.Workgroup.Z);
            writer.EndObject();
            writer.KeyUInt("targetTextByteLength", entryPoint.TargetText.size());
            writer.KeyUInt("targetTextHash", HashSourcePayload(entryPoint.TargetText));
            WriteIndexArray(writer, "usedBindingIndices", entryPoint.UsedBindingIndices);
            WriteVertexInputs(writer, entryPoint.Raster);
            WriteColorTargets(writer, entryPoint.Raster);
            writer.KeyBool("writesFragDepth", entryPoint.Raster.WritesFragDepth);
            writer.EndObject();
        }
        writer.EndArray();
    }

    void WriteExternDefaults(JsonWriter& writer, const RawModule& module)
    {
        writer.Key("externDefaults");
        writer.BeginArray();
        for (const ExternConstantDefault& entry : module.ExternDefaults)
        {
            writer.BeginObject();
            writer.KeyString("name", entry.Name);
            writer.KeyInt("value", entry.Value);
            writer.EndObject();
        }
        writer.EndArray();
    }

} // namespace

std::string MakeStageDumpFileName(std::string_view module_name, StageDumpKind kind)
{
    std::string name;
    name.reserve(module_name.size() + 24u);
    name.append(module_name);
    name.append(".stage-");
    name.append(ToString(kind));
    name.append(".json");
    return name;
}

std::string DumpPermutationSpace(std::string_view module_name, const PermutationSpace& space)
{
    JsonWriter writer{ true };
    writer.BeginObject();
    writer.KeyString("stage", "space");
    writer.KeyString("module", module_name);
    writer.KeyUInt("axisCount", space.size());
    writer.KeyUInt("spaceSize", ComputeVariantSpaceSize(space));

    writer.Key("axes");
    writer.BeginArray();
    for (const PermutationAxis* axis : space)
    {
        if (axis == nullptr)
        {
            continue;
        }

        WriteAxis(writer, *axis);
    }
    writer.EndArray();

    writer.EndObject();
    return FinishDocument(writer);
}

std::string DumpVariantSet(std::string_view module_name, const VariantSet& variant_set)
{
    JsonWriter writer{ true };
    writer.BeginObject();
    writer.KeyString("stage", "variants");
    writer.KeyString("module", module_name);
    writer.KeyUInt("variantCount", variant_set.Variants.size());
    writer.KeyUInt("spaceSize", variant_set.SpaceSize);

    writer.Key("variants");
    writer.BeginArray();
    for (const VariantDescriptor& descriptor : variant_set.Variants)
    {
        writer.BeginObject();
        writer.KeyUInt("index", descriptor.Index);
        writer.KeyString("suffix", MakeAssignmentSuffix(descriptor.Active));
        writer.KeyString("description", DescribeAssignment(descriptor.Canonical));
        writer.Key("active");
        WriteAssignment(writer, descriptor.Active);
        writer.Key("canonical");
        WriteAssignment(writer, descriptor.Canonical);
        writer.EndObject();
    }
    writer.EndArray();

    writer.EndObject();
    return FinishDocument(writer);
}

std::string DumpRawModule(const RawModule& module)
{
    JsonWriter writer{ true };
    writer.BeginObject();
    writer.KeyString("stage", "raw");
    writer.KeyString("module", module.Name);
    writer.KeyUInt("variantCount", module.Variants.size());

    writer.Key("entryPointNames");
    writer.BeginArray();
    for (const std::string& name : module.EntryPointNames)
    {
        writer.String(name);
    }
    writer.EndArray();

    WriteExternDefaults(writer, module);

    writer.Key("variants");
    writer.BeginArray();
    for (const RawVariant& variant : module.Variants)
    {
        writer.BeginObject();
        writer.KeyUInt("index", variant.VariantIndex);
        writer.KeyString("suffix", variant.VariantSuffix);
        writer.KeyString("description", variant.VariantDescription);
        WriteRawBindings(writer, variant);
        WriteRawSizeAttributes(writer, variant);
        WriteRawEntryPoints(writer, variant);
        writer.EndObject();
    }
    writer.EndArray();

    writer.EndObject();
    return FinishDocument(writer);
}

std::string DumpResolvedModule(std::string_view module_name, std::span<const CompiledVariant> variants)
{
    JsonWriter writer{ true };
    writer.BeginObject();
    writer.KeyString("stage", "resolved");
    writer.KeyString("module", module_name);
    writer.KeyUInt("variantCount", variants.size());

    writer.Key("variants");
    writer.BeginArray();
    for (const CompiledVariant& variant : variants)
    {
        writer.BeginObject();
        writer.KeyUInt("index", variant.VariantIndex);
        writer.KeyString("suffix", variant.VariantSuffix);
        writer.KeyString("description", variant.VariantDescription);

        writer.Key("globalBindings");
        writer.BeginArray();
        for (const ReflectedBinding& binding : variant.GlobalBindings)
        {
            WriteBinding(writer, binding);
        }
        writer.EndArray();

        writer.Key("entryPoints");
        writer.BeginArray();
        for (const CompiledEntryPoint& entryPoint : variant.EntryPoints)
        {
            writer.BeginObject();
            writer.KeyString("name", entryPoint.Name);
            writer.KeyString("stage", magic_enum::enum_name(entryPoint.Reflection.Stage));
            writer.Key("workgroup");
            writer.BeginObject();
            writer.KeyUInt("x", entryPoint.Reflection.Workgroup.X);
            writer.KeyUInt("y", entryPoint.Reflection.Workgroup.Y);
            writer.KeyUInt("z", entryPoint.Reflection.Workgroup.Z);
            writer.EndObject();
            writer.KeyUInt("targetTextByteLength", entryPoint.Code.size());
            writer.KeyUInt("targetTextHash", HashSourcePayload(entryPoint.Code));
            WriteIndexArray(writer, "usedBindingIndices", entryPoint.Reflection.UsedBindingIndices);
            WriteVertexInputs(writer, entryPoint.Reflection.Raster);
            WriteColorTargets(writer, entryPoint.Reflection.Raster);
            writer.KeyBool("writesFragDepth", entryPoint.Reflection.Raster.WritesFragDepth);
            writer.EndObject();
        }
        writer.EndArray();

        writer.EndObject();
    }
    writer.EndArray();

    writer.EndObject();
    return FinishDocument(writer);
}

namespace
{

    /** Every artifact that mapped onto one unique entry, for one table.
     *
     * This is the only thing the `interned` dump shows that `cooked` cannot. The interner records who
     * mapped where, `FreezeModuleTables` copies out the unique entries and leaves the provenance
     * behind, so after the freeze the answer is gone. A collapse that surprises you is findable here
     * and nowhere else. */
    template<typename PayloadType>
    void WriteProvenance(JsonWriter& writer,
                         std::string_view key,
                         const ContentInterner<PayloadType>& interner)
    {
        writer.Key(key);
        writer.BeginArray();
        for (uint32_t index = 0u; index < interner.UniqueEntries().size(); ++index)
        {
            writer.BeginObject();
            writer.KeyUInt("index", index);
            writer.Key("mappedFrom");
            writer.BeginArray();
            for (const ProvenanceRecord& origin : interner.OriginsOf(index))
            {
                writer.BeginObject();
                writer.KeyString("entryPoint", origin.EntryPointName);
                writer.KeyString("variant", origin.VariantDescription);
                writer.KeyUInt("variantIndex", origin.VariantIndex);
                writer.EndObject();
            }
            writer.EndArray();
            writer.EndObject();
        }
        writer.EndArray();
    }

} // namespace

std::string DumpInternedModule(const InternedModule& module)
{
    JsonWriter writer{ true };
    writer.BeginObject();
    writer.KeyString("stage", "interned");
    writer.KeyString("module", module.Name);
    writer.KeyUInt("spaceSize", module.SpaceSize);
    writer.KeyUInt("variantCount", module.Variants.size());

    WriteEntryPointTable(writer, module.EntryPoints);
    WriteVariantTable(writer, module.Variants);

    writer.Key("interners");
    writer.BeginObject();
    WriteInterner(writer,
                  "sources",
                  module.SourceInterner.HashName(),
                  module.SourceInterner.IsEnabled(),
                  module.SourceInterner.Statistics());
    WriteInterner(writer,
                  "layouts",
                  module.LayoutInterner.HashName(),
                  module.LayoutInterner.IsEnabled(),
                  module.LayoutInterner.Statistics());
    WriteInterner(writer,
                  "rasterStates",
                  module.RasterInterner.HashName(),
                  module.RasterInterner.IsEnabled(),
                  module.RasterInterner.Statistics());
    writer.EndObject();

    writer.Key("provenance");
    writer.BeginObject();
    WriteProvenance(writer, "sources", module.SourceInterner);
    WriteProvenance(writer, "layouts", module.LayoutInterner);
    WriteProvenance(writer, "rasterStates", module.RasterInterner);
    writer.EndObject();

    writer.EndObject();
    return FinishDocument(writer);
}

std::string DumpCookedModule(const CookedModule& module)
{
    JsonWriter writer{ true };
    writer.BeginObject();
    writer.KeyString("stage", "cooked");
    writer.KeyString("module", module.Name);
    writer.KeyUInt("spaceSize", module.SpaceSize);
    writer.KeyUInt("sourceCount", module.Sources.size());
    writer.KeyUInt("layoutCount", module.Layouts.size());
    writer.KeyUInt("rasterStateCount", module.RasterStates.size());
    writer.KeyUInt("variantCount", module.Variants.size());

    WriteEntryPointTable(writer, module.EntryPoints);
    WriteSourceTable(writer, module);
    WriteLayoutTable(writer, module);
    WriteRasterTable(writer, module);
    WriteVariantTable(writer, module.Variants);
    WriteInternerTable(writer, module);

    writer.EndObject();
    return FinishDocument(writer);
}

} // namespace lodestone
