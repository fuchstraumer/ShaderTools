#include "SlangCompiler.hpp"
#include "CookerErrors.hpp"
#include "Diagnostics.hpp"
#include "PermutationSpace.hpp"
#include "ResourceFlags.hpp"
#include "ShaderDataSchema.hpp"
#include "ShaderLibraryTypes.hpp"
#include "SlangDiagnosticParser.hpp"

#include <filesystem>
#include <ios>
#include <memory>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <fstream>
#include <future>
#include <iterator>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lodestone
{

namespace
{

    constexpr SlangInt k_WgslTargetIndex = 0;
    constexpr const char* k_AllWarningsAsErrors = "all";
    constexpr const char* k_DisabledWarnings = "31010";
    constexpr SlangUInt k_WorkgroupAxisCount = 3u;

    // The shader-side attribute names live on RawSizeAttributeKind, so one table states the contract
    // and this file asks it for the spelling. Slang drops the `Attribute` suffix from the struct name,
    // so those strings match the declarations in tests/assets/LodestoneAttributes.slang.
    constexpr std::array<RawSizeAttributeKind, 3u> k_SizeAttributeKinds{ RawSizeAttributeKind::ElementCount,
                                                                         RawSizeAttributeKind::Extent2d,
                                                                         RawSizeAttributeKind::Extent3d };

    /** Attribute string arguments reflect as a pointer plus a length, and a null return means the
     * argument was not a string literal after all. That is a cook error rather than a skip: the
     * annotation was written, so the author expects it to do something. */
    CookResult<std::string> ReadStringArgument(slang::Attribute* attribute,
                                               uint32_t argument_index,
                                               std::string_view binding_name)
    {
        size_t length = 0u;
        const char* text = attribute->getArgumentValueString(argument_index, &length);
        if (text == nullptr)
        {
            std::println(stderr,
                         "[shader_cooker] argument {} of [{}] on '{}' is not a string literal",
                         argument_index,
                         attribute->getName(),
                         binding_name);
            return std::unexpected(CookError::SizeExpressionParseFailed);
        }

        // The reflected span includes the surrounding quotes on some paths; trim them if present.
        std::string_view value{ text, length };
        if (value.size() >= 2u && value.front() == '"' && value.back() == '"')
        {
            value = value.substr(1u, value.size() - 2u);
        }

        return std::string{ value };
    }

    std::string BlobToString(slang::IBlob* blob)
    {
        if (blob == nullptr)
        {
            return {};
        }

        return std::string{ static_cast<const char*>(blob->getBufferPointer()), blob->getBufferSize() };
    }

    void ReportDiagnosticText(DiagnosticSink& sink, std::string_view context, std::string_view text)
    {
        if (text.empty())
        {
            return;
        }

        ParseSlangDiagnostics(text, context, sink);
    }

    void ReportDiagnostics(DiagnosticSink& sink, std::string_view context, slang::IBlob* blob)
    {
        if (blob == nullptr || blob->getBufferSize() == 0u)
        {
            return;
        }

        ReportDiagnosticText(sink, context, BlobToString(blob));
    }

    /** What one entry point's codegen produced. The diagnostic text travels with the code so that a
     * worker thread never touches a sink. coalesced after threads join */
    struct GeneratedEntryPoint
    {
        std::string Code;
        std::string Diagnostics;
    };

    GeneratedEntryPoint GenerateOneEntryPoint(slang::IComponentType* linked_program, size_t index)
    {
        Slang::ComPtr<slang::IBlob> code;
        Slang::ComPtr<slang::IBlob> diagnostics;
        const bool failed = SLANG_FAILED(linked_program->getEntryPointCode(
            static_cast<SlangInt>(index), k_WgslTargetIndex, code.writeRef(), diagnostics.writeRef()));

        return GeneratedEntryPoint{ .Code = failed ? std::string{} : BlobToString(code.get()),
                                    .Diagnostics = BlobToString(diagnostics.get()) };
    }

    SlangOptimizationLevel ToSlangOptimizationLevel(uint32_t level) noexcept
    {
        switch (level)
        {
        case 1u:
            return SLANG_OPTIMIZATION_LEVEL_DEFAULT;
        case 2u:
            return SLANG_OPTIMIZATION_LEVEL_HIGH;
        case 3u:
            return SLANG_OPTIMIZATION_LEVEL_MAXIMAL;
        default:
            return SLANG_OPTIMIZATION_LEVEL_NONE;
        }
    }

    ShaderStageKind FromSlangStage(SlangStage stage) noexcept
    {
        switch (stage)
        {
        case SLANG_STAGE_VERTEX:
            return ShaderStageKind::Vertex;
        case SLANG_STAGE_FRAGMENT:
            return ShaderStageKind::Fragment;
        case SLANG_STAGE_COMPUTE:
            return ShaderStageKind::Compute;
        default:
            return ShaderStageKind::Invalid;
        }
    }

    BindingKind FromSlangBindingType(slang::BindingType binding_type) noexcept
    {
        switch (binding_type)
        {
        case slang::BindingType::Sampler:
            return BindingKind::Sampler;
        case slang::BindingType::Texture:
            return BindingKind::SampledTexture;
        case slang::BindingType::MutableTexture:
            return BindingKind::StorageTexture;
        case slang::BindingType::ConstantBuffer:
        case slang::BindingType::ParameterBlock:
        case slang::BindingType::InlineUniformData:
            return BindingKind::UniformBuffer;
        case slang::BindingType::RawBuffer:
        case slang::BindingType::TypedBuffer:
            return BindingKind::ReadOnlyStorageBuffer;
        case slang::BindingType::MutableRawBuffer:
        case slang::BindingType::MutableTypedBuffer:
            return BindingKind::StorageBuffer;
        default:
            return BindingKind::Invalid;
        }
    }

    /** Slang packs the base shape and the array and multisample flags into one value, so the base
     * shape must be masked out before the comparison. */
    ResourceShape FromSlangResourceShape(SlangResourceShape shape) noexcept
    {
        const SlangResourceShape baseShape =
            static_cast<SlangResourceShape>(shape & SLANG_RESOURCE_BASE_SHAPE_MASK);
        const bool isArray = (shape & SLANG_TEXTURE_ARRAY_FLAG) != 0;
        const bool isMultisample = (shape & SLANG_TEXTURE_MULTISAMPLE_FLAG) != 0;

        switch (baseShape)
        {
        case SLANG_TEXTURE_1D:
            return ResourceShape::Texture1D;
        case SLANG_TEXTURE_2D:
            if (isMultisample)
            {
                return ResourceShape::Texture2DMultisample;
            }
            return isArray ? ResourceShape::Texture2DArray : ResourceShape::Texture2D;
        case SLANG_TEXTURE_3D:
            return ResourceShape::Texture3D;
        case SLANG_TEXTURE_CUBE:
            return isArray ? ResourceShape::TextureCubeArray : ResourceShape::TextureCube;
        case SLANG_STRUCTURED_BUFFER:
        case SLANG_BYTE_ADDRESS_BUFFER:
        case SLANG_TEXTURE_BUFFER:
            return ResourceShape::Buffer;
        default:
            return ResourceShape::Invalid;
        }
    }

    /** Maps the scalar type a texture returns onto the sample type WebGPU wants. A depth texture is
     * not distinguishable here, so the graph decides that from the format it creates. */
    TextureSampleType FromSlangScalarType(slang::TypeReflection::ScalarType scalar_type) noexcept
    {
        switch (scalar_type)
        {
        case slang::TypeReflection::ScalarType::Int8:
        case slang::TypeReflection::ScalarType::Int16:
        case slang::TypeReflection::ScalarType::Int32:
        case slang::TypeReflection::ScalarType::Int64:
            return TextureSampleType::SignedInteger;
        case slang::TypeReflection::ScalarType::UInt8:
        case slang::TypeReflection::ScalarType::UInt16:
        case slang::TypeReflection::ScalarType::UInt32:
        case slang::TypeReflection::ScalarType::UInt64:
            return TextureSampleType::UnsignedInteger;
        case slang::TypeReflection::ScalarType::Float16:
        case slang::TypeReflection::ScalarType::Float32:
        case slang::TypeReflection::ScalarType::Float64:
            return TextureSampleType::Float;
        default:
            return TextureSampleType::Invalid;
        }
    }

    VertexScalarType FromSlangVertexScalarType(slang::TypeReflection::ScalarType scalar_type) noexcept
    {
        switch (scalar_type)
        {
        case slang::TypeReflection::ScalarType::Float16:
            return VertexScalarType::Float16;
        case slang::TypeReflection::ScalarType::Float32:
            return VertexScalarType::Float32;
        case slang::TypeReflection::ScalarType::Int32:
            return VertexScalarType::SignedInteger32;
        case slang::TypeReflection::ScalarType::UInt32:
            return VertexScalarType::UnsignedInteger32;
        default:
            return VertexScalarType::Invalid;
        }
    }

    /** Splits one varying into a scalar type and a component count. A scalar counts as one component,
     * so a caller never has to test the kind again. */
    void ReadVaryingShape(slang::TypeLayoutReflection* type_layout,
                          VertexScalarType& out_scalar_type,
                          uint32_t& out_component_count) noexcept
    {
        out_scalar_type = VertexScalarType::Invalid;
        out_component_count = 0u;

        if (type_layout == nullptr)
        {
            return;
        }

        if (type_layout->getKind() == slang::TypeReflection::Kind::Vector)
        {
            slang::TypeLayoutReflection* elementLayout = type_layout->getElementTypeLayout();
            out_component_count = static_cast<uint32_t>(type_layout->getElementCount());
            if (elementLayout != nullptr)
            {
                out_scalar_type = FromSlangVertexScalarType(elementLayout->getType()->getScalarType());
            }
            return;
        }

        if (type_layout->getKind() == slang::TypeReflection::Kind::Scalar)
        {
            out_component_count = 1u;
            out_scalar_type = FromSlangVertexScalarType(type_layout->getType()->getScalarType());
        }
    }

    /** True for a name the hardware supplies, such as SV_Position. Those never become a vertex buffer
     * attribute, so they must not reach the vertex input list. */
    bool IsSystemSemantic(std::string_view semantic_name) noexcept
    {
        return semantic_name.starts_with("SV_") || semantic_name.starts_with("sv_");
    }

    bool IsDepthSemantic(std::string_view semantic_name) noexcept
    {
        return semantic_name == "SV_Depth" || semantic_name == "SV_DEPTH" ||
               semantic_name == "SV_DepthGreaterEqual" || semantic_name == "SV_DepthLessEqual";
    }

    /** Flattens one uniform block into rows of name, offset, and size. A nested struct recurses and
     * its fields take a dotted name, because the consumer compares offsets and a tree would only make
     * that harder. */
    void CollectUniformMembers(slang::TypeLayoutReflection* struct_layout,
                               const std::string& name_prefix,
                               uint32_t base_offset,
                               std::vector<ReflectedUniformMember>& members)
    {
        if (struct_layout == nullptr || struct_layout->getKind() != slang::TypeReflection::Kind::Struct)
        {
            return;
        }

        const unsigned int fieldCount = struct_layout->getFieldCount();
        for (unsigned int i = 0u; i < fieldCount; ++i)
        {
            slang::VariableLayoutReflection* field = struct_layout->getFieldByIndex(i);
            if (field == nullptr)
            {
                continue;
            }

            const char* fieldName = field->getName();
            if (fieldName == nullptr)
            {
                continue;
            }

            const uint32_t offset =
                base_offset + static_cast<uint32_t>(field->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM));
            const std::string qualifiedName =
                name_prefix.empty() ? std::string{ fieldName } : name_prefix + "." + fieldName;

            slang::TypeLayoutReflection* fieldLayout = field->getTypeLayout();
            if (fieldLayout == nullptr)
            {
                continue;
            }

            if (fieldLayout->getKind() == slang::TypeReflection::Kind::Struct)
            {
                CollectUniformMembers(fieldLayout, qualifiedName, offset, members);
                continue;
            }

            ReflectedUniformMember member;
            member.Name = qualifiedName;
            member.Offset = offset;
            member.Size = static_cast<uint32_t>(fieldLayout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
            member.ArrayCount = fieldLayout->getKind() == slang::TypeReflection::Kind::Array
                                    ? static_cast<uint32_t>(fieldLayout->getElementCount())
                                    : 1u;

            members.push_back(std::move(member));
        }
    }

    std::string_view ReadSemanticName(slang::VariableLayoutReflection* var_layout) noexcept
    {
        const char* name = var_layout->getSemanticName();
        if (name == nullptr)
        {
            return {};
        }

        return std::string_view{ name };
    }

    /** Walks the vertex entry point parameters and records every attribute a vertex buffer must
     * supply. Nested structs recurse, and the location accumulates down the tree, because Slang
     * states each field's offset against its parent. */
    void CollectVertexInputs(slang::VariableLayoutReflection* var_layout,
                             uint32_t base_location,
                             ReflectedRasterState& raster)
    {
        if (var_layout == nullptr)
        {
            return;
        }

        slang::TypeLayoutReflection* typeLayout = var_layout->getTypeLayout();
        if (typeLayout == nullptr)
        {
            return;
        }

        const uint32_t location =
            base_location +
            static_cast<uint32_t>(var_layout->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_INPUT));

        if (typeLayout->getKind() == slang::TypeReflection::Kind::Struct)
        {
            const unsigned int fieldCount = typeLayout->getFieldCount();
            for (unsigned int i = 0u; i < fieldCount; ++i)
            {
                CollectVertexInputs(typeLayout->getFieldByIndex(i), location, raster);
            }
            return;
        }

        const std::string_view semantic = ReadSemanticName(var_layout);
        if (semantic.empty() || IsSystemSemantic(semantic))
        {
            return;
        }

        ReflectedVertexInput input;
        input.SemanticName = std::string{ semantic };
        input.Data.SemanticIndex = static_cast<uint32_t>(var_layout->getSemanticIndex());
        input.Data.Location = location;
        ReadVaryingShape(typeLayout, input.Data.ScalarType, input.Data.ComponentCount);

        raster.VertexInputs.push_back(std::move(input));
    }

    /** Walks the fragment entry point result and records every color target, plus whether the shader
     * writes depth itself. */
    void CollectColorTargets(slang::VariableLayoutReflection* var_layout,
                             uint32_t base_location,
                             ReflectedRasterState& raster)
    {
        if (var_layout == nullptr)
        {
            return;
        }

        slang::TypeLayoutReflection* typeLayout = var_layout->getTypeLayout();
        if (typeLayout == nullptr)
        {
            return;
        }

        const uint32_t location =
            base_location +
            static_cast<uint32_t>(var_layout->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_OUTPUT));

        if (typeLayout->getKind() == slang::TypeReflection::Kind::Struct)
        {
            const unsigned int fieldCount = typeLayout->getFieldCount();
            for (unsigned int i = 0u; i < fieldCount; ++i)
            {
                CollectColorTargets(typeLayout->getFieldByIndex(i), location, raster);
            }
            return;
        }

        const std::string_view semantic = ReadSemanticName(var_layout);
        if (IsDepthSemantic(semantic))
        {
            raster.WritesFragDepth = true;
            return;
        }

        ReflectedColorTarget target;
        target.Location = location;
        ReadVaryingShape(typeLayout, target.ScalarType, target.ComponentCount);

        if (target.ComponentCount == 0u)
        {
            return;
        }

        raster.ColorTargets.push_back(target);
    }

    void CollectDepthWrites(slang::VariableLayoutReflection* var_layout, ReflectedRasterState& raster)
    {
        if (var_layout == nullptr)
        {
            return;
        }

        slang::TypeLayoutReflection* typeLayout = var_layout->getTypeLayout();
        if (typeLayout == nullptr)
        {
            return;
        }

        if (typeLayout->getKind() == slang::TypeReflection::Kind::Struct)
        {
            const unsigned int fieldCount = typeLayout->getFieldCount();
            for (unsigned int i = 0u; i < fieldCount; ++i)
            {
                CollectDepthWrites(typeLayout->getFieldByIndex(i), raster);
            }
            return;
        }

        if (IsDepthSemantic(ReadSemanticName(var_layout)))
        {
            raster.WritesFragDepth = true;
        }
    }

    /** Only the formats Velox curates. An unmapped format returns Invalid on purpose: the graph must
     * reject a shader that asks for a format the engine does not express. */
    TextureFormat FromSlangImageFormat(SlangImageFormat format) noexcept
    {
        switch (format)
        {
        case SLANG_IMAGE_FORMAT_r8:
            return TextureFormat::R8Unorm;
        case SLANG_IMAGE_FORMAT_rg8:
            return TextureFormat::Rg8Unorm;
        case SLANG_IMAGE_FORMAT_rgba8:
            return TextureFormat::Rgba8Unorm;
        case SLANG_IMAGE_FORMAT_r16f:
            return TextureFormat::R16Float;
        case SLANG_IMAGE_FORMAT_rg16f:
            return TextureFormat::Rg16Float;
        case SLANG_IMAGE_FORMAT_rgba16f:
            return TextureFormat::Rgba16Float;
        case SLANG_IMAGE_FORMAT_r32f:
            return TextureFormat::R32Float;
        case SLANG_IMAGE_FORMAT_rg32f:
            return TextureFormat::Rg32Float;
        case SLANG_IMAGE_FORMAT_rgba32f:
            return TextureFormat::Rgba32Float;
        case SLANG_IMAGE_FORMAT_r32ui:
            return TextureFormat::R32Uint;
        case SLANG_IMAGE_FORMAT_rg32ui:
            return TextureFormat::Rg32Uint;
        case SLANG_IMAGE_FORMAT_rgba32ui:
            return TextureFormat::Rgba32Uint;
        case SLANG_IMAGE_FORMAT_rgb10_a2:
            return TextureFormat::Rgb10A2Unorm;
        case SLANG_IMAGE_FORMAT_r11f_g11f_b10f:
            return TextureFormat::Rg11B10Ufloat;
        default:
            return TextureFormat::Invalid;
        }
    }

    StorageTextureAccess FromSlangBindingTypeAccess(slang::BindingType binding_type) noexcept
    {
        switch (binding_type)
        {
        case slang::BindingType::MutableTexture:
            return StorageTextureAccess::ReadWrite;
        case slang::BindingType::Texture:
            return StorageTextureAccess::ReadOnly;
        default:
            return StorageTextureAccess::Invalid;
        }
    }

    std::vector<slang::CompilerOptionEntry> MakeCompilerOptions(uint32_t optimization_level)
    {
        std::vector<slang::CompilerOptionEntry> options;
        options.reserve(6u);

        slang::CompilerOptionEntry warningLevel{};
        warningLevel.name = slang::CompilerOptionName::WarningLevel;
        warningLevel.value.kind = slang::CompilerOptionValueKind::Int;
        warningLevel.value.intValue0 = SLANG_WARNING_LEVEL_PEDANTIC;
        options.push_back(warningLevel);
        warningLevel.value.intValue0 = SLANG_WARNING_LEVEL_ALL;
        options.push_back(warningLevel);

        slang::CompilerOptionEntry disableWarnings{};
        disableWarnings.name = slang::CompilerOptionName::DisableWarnings;
        disableWarnings.value.kind = slang::CompilerOptionValueKind::String;
        disableWarnings.value.stringValue0 = k_DisabledWarnings;
        options.push_back(disableWarnings);

        slang::CompilerOptionEntry warningsAsErrors{};
        warningsAsErrors.name = slang::CompilerOptionName::WarningsAsErrors;
        warningsAsErrors.value.kind = slang::CompilerOptionValueKind::String;
        warningsAsErrors.value.stringValue0 = k_AllWarningsAsErrors;
        options.push_back(warningsAsErrors);

        slang::CompilerOptionEntry floatingPointMode{};
        floatingPointMode.name = slang::CompilerOptionName::FloatingPointMode;
        floatingPointMode.value.kind = slang::CompilerOptionValueKind::Int;
        floatingPointMode.value.intValue0 = SLANG_FLOATING_POINT_MODE_FAST;
        options.push_back(floatingPointMode);

        slang::CompilerOptionEntry debugInformation{};
        debugInformation.name = slang::CompilerOptionName::DebugInformation;
        debugInformation.value.kind = slang::CompilerOptionValueKind::Int;
        debugInformation.value.intValue0 = SLANG_DEBUG_INFO_LEVEL_NONE;
        options.push_back(debugInformation);

        slang::CompilerOptionEntry machineReadable{};
        machineReadable.name = slang::CompilerOptionName::EnableMachineReadableDiagnostics;
        machineReadable.value.kind = slang::CompilerOptionValueKind::Int;
        machineReadable.value.intValue0 = 1;
        options.push_back(machineReadable);

        slang::CompilerOptionEntry optimization{};
        optimization.name = slang::CompilerOptionName::Optimization;
        optimization.value.kind = slang::CompilerOptionValueKind::Int;
        optimization.value.intValue0 = static_cast<int32_t>(ToSlangOptimizationLevel(optimization_level));
        options.push_back(optimization);

        return options;
    }

} // namespace

struct SlangCompiler::Impl
{
    Slang::ComPtr<slang::IGlobalSession> GlobalSession;
    Slang::ComPtr<slang::ISession> Session;
    slang::IModule* RootModule{ nullptr };
    std::vector<slang::IComponentType*> BaseComponents;
    std::vector<Slang::ComPtr<slang::IEntryPoint>> EntryPointHandles;
    std::vector<std::string> EntryPointNames;
    std::vector<slang::CompilerOptionEntry> CompilerOptions;
    std::vector<std::string> ModuleSourceTexts;
    std::string ModuleName;
    bool MultithreadEntryPointCodegen{ true };
    /** Set once, by `Initialize`, and never null after that. A pointer rather than a reference only
     * because this object moves. */
    DiagnosticSink* Sink{ nullptr };

    CookResult<void> CreateSession(const SlangCompilerCreateInfo& create_info);
    CookResult<void> LoadRootModule();
    void ReadDependencySourceTexts();
    CookResult<void> CollectEntryPoints();
    CookResult<Slang::ComPtr<slang::IComponentType>> LinkVariant(
        const PermutationAssignment& assignment) const;
    std::vector<std::string> GenerateEntryPointCode(slang::IComponentType* linked_program) const;
    CookResult<RawEntryPoint> ExtractRawEntryPoint(slang::IComponentType* linked_program,
                                                   slang::ProgramLayout* program_layout,
                                                   SlangInt entry_point_index,
                                                   const std::vector<RawBinding>& global_bindings);
    CookResult<void> ExtractRawBindings(slang::ProgramLayout* program_layout,
                                        std::vector<RawBinding>& out_bindings,
                                        std::vector<RawSizeAttribute>& out_attributes);
    CookResult<void> CollectRawSizeAttributes(slang::VariableReflection* leaf_variable,
                                              std::string_view binding_name,
                                              std::vector<RawSizeAttribute>& out_attributes) const;
    static void ApplyLeafTypeLayout(slang::TypeLayoutReflection* global_layout,
                                    SlangInt range_index,
                                    slang::BindingType binding_type,
                                    RawBinding& binding);
    // Not static: it reports through the sink, which is a member.
    void CollectUsedBindingIndices(slang::IComponentType* linked_program,
                                   SlangInt entry_point_index,
                                   const std::vector<RawBinding>& global_bindings,
                                   std::vector<uint32_t>& out_used_indices);
    static void ExtractRasterState(slang::EntryPointReflection* entry_point_layout,
                                   ShaderStageKind stage,
                                   ReflectedRasterState& raster);
};

CookResult<void> SlangCompiler::Impl::CreateSession(const SlangCompilerCreateInfo& create_info)
{
    if (SLANG_FAILED(slang::createGlobalSession(GlobalSession.writeRef())) || GlobalSession == nullptr)
    {
        return std::unexpected(CookError::GlobalSessionCreationFailed);
    }

    CompilerOptions = MakeCompilerOptions(create_info.OptimizationLevel);
    // todo-asap: I am inserting the tests/assets/ rootdir here for the attributes file. This needs to be
    // optionalized and standardized
    const std::filesystem::path attributesPath = std::filesystem::canonical("D:/ShaderTools/tests/assets/");
    const std::string attributesPathStr = attributesPath.string();
    const std::filesystem::path canonicalModulePath = std::filesystem::canonical(create_info.ModulePath);
    const std::string sourceDirectory = canonicalModulePath.parent_path().string();
    // The shared modules a shader imports -- VeloxAttributes among them -- sit one level above the
    // per-stage directory, so the asset root resolves without a command-line switch.
    const std::string sharedDirectory = canonicalModulePath.parent_path().parent_path().string();
    const std::string cacheDirectory = create_info.ModuleCacheDirectory.string();
    const std::array<const char*, 4> searchPaths{
        sourceDirectory.c_str(), sharedDirectory.c_str(), cacheDirectory.c_str(), attributesPathStr.c_str()
    };

    slang::TargetDesc target{};
    target.format = SLANG_WGSL;
    target.profile = GlobalSession->findProfile("spirv_1_4");

    slang::SessionDesc sessionDesc{};
    sessionDesc.targets = &target;
    sessionDesc.targetCount = 1;
    sessionDesc.searchPaths = searchPaths.data();
    sessionDesc.searchPathCount = static_cast<SlangInt>(searchPaths.size());
    sessionDesc.compilerOptionEntries = CompilerOptions.data();
    sessionDesc.compilerOptionEntryCount = static_cast<SlangInt>(CompilerOptions.size());

    if (SLANG_FAILED(GlobalSession->createSession(sessionDesc, Session.writeRef())) || Session == nullptr)
    {
        return std::unexpected(CookError::SessionCreationFailed);
    }

    ModuleName = canonicalModulePath.stem().string();
    MultithreadEntryPointCodegen = create_info.MultithreadEntryPointCodegen;
    return {};
}

CookResult<void> SlangCompiler::Impl::LoadRootModule()
{
    Slang::ComPtr<slang::IBlob> diagnostics;
    RootModule = Session->loadModule(ModuleName.c_str(), diagnostics.writeRef());
    ReportDiagnostics(*Sink, "loadModule", diagnostics.get());

    if (RootModule == nullptr)
    {
        return std::unexpected(CookError::ModuleLoadFailed);
    }

    BaseComponents.clear();
    BaseComponents.reserve(4u + static_cast<size_t>(RootModule->getDefinedEntryPointCount()));
    BaseComponents.push_back(RootModule);

    ReadDependencySourceTexts();
    return {};
}

/** Slang reports every file the module pulled in, transitively. That set is what the axis-name check
 * searches, and it is also the right input for a future content hash driving live reload. */
void SlangCompiler::Impl::ReadDependencySourceTexts()
{
    const SlangInt32 dependencyCount = RootModule->getDependencyFileCount();
    ModuleSourceTexts.clear();
    ModuleSourceTexts.reserve(static_cast<size_t>(dependencyCount));

    for (SlangInt32 i = 0; i < dependencyCount; ++i)
    {
        const char* dependencyPath = RootModule->getDependencyFilePath(i);
        if (dependencyPath == nullptr)
        {
            continue;
        }

        std::ifstream file{ dependencyPath, std::ios::binary };
        if (!file)
        {
            std::println(stderr, "[shader_cooker] could not read dependency {}", dependencyPath);
            continue;
        }

        ModuleSourceTexts.emplace_back(std::istreambuf_iterator<char>{ file },
                                       std::istreambuf_iterator<char>{});
    }
}

CookResult<void> SlangCompiler::Impl::CollectEntryPoints()
{
    const SlangInt entryPointCount = RootModule->getDefinedEntryPointCount();
    EntryPointNames.clear();
    EntryPointNames.reserve(static_cast<size_t>(entryPointCount));
    EntryPointHandles.clear();
    EntryPointHandles.reserve(static_cast<size_t>(entryPointCount));

    for (SlangInt i = 0; i < entryPointCount; ++i)
    {
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        if (SLANG_FAILED(RootModule->getDefinedEntryPoint(i, entryPoint.writeRef())))
        {
            return std::unexpected(CookError::EntryPointEnumerationFailed);
        }

        EntryPointNames.emplace_back(entryPoint->getFunctionReflection()->getName());
        BaseComponents.push_back(entryPoint.get());
        EntryPointHandles.push_back(entryPoint);
    }

    return {};
}

CookResult<Slang::ComPtr<slang::IComponentType>> SlangCompiler::Impl::LinkVariant(
    const PermutationAssignment& assignment) const
{
    std::vector<slang::IComponentType*> components = BaseComponents;
    components.reserve(BaseComponents.size() + assignment.size());

    for (const PermutationBinding& binding : assignment)
    {
        const std::string variantModuleName = MakeVariantModuleName(binding.first->Name, binding.second);
        const std::string variantModulePath = MakeVariantModulePath(binding.first->Name, binding.second);
        const std::string variantSource = MakeExportedConstantSource(binding.first->Name, binding.second);

        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* variantModule = Session->loadModuleFromSourceString(variantModuleName.c_str(),
                                                                            variantModulePath.c_str(),
                                                                            variantSource.c_str(),
                                                                            diagnostics.writeRef());
        ReportDiagnostics(*Sink, "loadModuleFromSourceString", diagnostics.get());

        if (variantModule == nullptr)
        {
            return std::unexpected(CookError::VariantModuleCreationFailed);
        }

        components.push_back(variantModule);
    }

    Slang::ComPtr<slang::IBlob> diagnostics;
    Slang::ComPtr<slang::IComponentType> composite;
    Session->createCompositeComponentType(components.data(),
                                          static_cast<SlangInt>(components.size()),
                                          composite.writeRef(),
                                          diagnostics.writeRef());
    ReportDiagnostics(*Sink, "createCompositeComponentType", diagnostics.get());

    if (composite == nullptr)
    {
        return std::unexpected(CookError::CompositeCreationFailed);
    }

    Slang::ComPtr<slang::IComponentType> linked;
    if (SLANG_FAILED(composite->link(linked.writeRef(), diagnostics.writeRef())))
    {
        ReportDiagnostics(*Sink, "link", diagnostics.get());
        return std::unexpected(CookError::LinkFailed);
    }

    return linked;
}

std::vector<std::string> SlangCompiler::Impl::GenerateEntryPointCode(
    slang::IComponentType* linked_program) const
{
    const size_t entryPointCount = EntryPointNames.size();
    std::vector<std::string> generated(entryPointCount);

    if (MultithreadEntryPointCodegen)
    {
        // A worker carries its diagnostic text back rather than reporting it. Two reasons: a sink has
        // no lock, and a message reported from a worker would arrive in whatever order the threads
        // finished. Reporting on the joining thread puts every message in entry point order, so two
        // runs of one cook say the same thing in the same sequence.
        std::vector<std::future<GeneratedEntryPoint>> pending;
        pending.reserve(entryPointCount);

        for (size_t i = 0; i < entryPointCount; ++i)
        {
            pending.push_back(std::async(std::launch::async,
                                         [linked_program, i]()
                                         {
                                             return GenerateOneEntryPoint(linked_program, i);
                                         }));
        }

        for (size_t i = 0; i < entryPointCount; ++i)
        {
            GeneratedEntryPoint result = pending[i].get();
            ReportDiagnosticText(*Sink, "getEntryPointCode", result.Diagnostics);
            generated[i] = std::move(result.Code);
        }

        return generated;
    }

    for (size_t i = 0; i < entryPointCount; ++i)
    {
        GeneratedEntryPoint result = GenerateOneEntryPoint(linked_program, i);
        ReportDiagnosticText(*Sink, "getEntryPointCode", result.Diagnostics);
        generated[i] = std::move(result.Code);
    }

    return generated;
}

/** Reads the size, shape, and type facts off one binding range's leaf type layout.
 *
 * Slang wraps a resource type around the type it carries, so the useful facts sit one level down.
 * A structured buffer reports its element layout. A texture reports the type it returns. */
void SlangCompiler::Impl::ApplyLeafTypeLayout(slang::TypeLayoutReflection* global_layout,
                                              SlangInt range_index,
                                              slang::BindingType binding_type,
                                              RawBinding& binding)
{
    slang::TypeLayoutReflection* leafLayout = global_layout->getBindingRangeLeafTypeLayout(range_index);
    if (leafLayout == nullptr)
    {
        return;
    }

    if (binding.Kind == BindingKind::StorageTexture)
    {
        binding.StorageFormat = FromSlangImageFormat(global_layout->getBindingRangeImageFormat(range_index));
        binding.StorageAccess = FromSlangBindingTypeAccess(binding_type);
    }

    if (binding.Kind == BindingKind::Sampler)
    {
        binding.Shape = ResourceShape::Invalid;
        binding.SamplerType = SamplerBindingType::Filtering;
        return;
    }

    slang::TypeReflection* leafType = leafLayout->getType();
    if (leafType == nullptr)
    {
        return;
    }

    binding.Shape = FromSlangResourceShape(leafType->getResourceShape());

    if (IsTextureBinding(binding.Kind))
    {
        slang::TypeReflection* resultType = leafType->getResourceResultType();
        if (resultType != nullptr)
        {
            binding.SampleType = FromSlangScalarType(resultType->getScalarType());
        }

        return;
    }

    if (binding.Kind == BindingKind::UniformBuffer)
    {
        // A uniform block's size is fully determined. The graph must never take it from the caller.
        binding.ByteSize = static_cast<uint64_t>(leafLayout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
        slang::TypeLayoutReflection* elementLayout = leafLayout->getElementTypeLayout();
        if (elementLayout != nullptr && binding.ByteSize == 0u)
        {
            binding.ByteSize =
                static_cast<uint64_t>(elementLayout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
        }

        CollectUniformMembers(elementLayout, std::string{}, 0u, binding.UniformMembers);
        return;
    }

    slang::TypeLayoutReflection* elementLayout = leafLayout->getElementTypeLayout();
    if (elementLayout != nullptr)
    {
        binding.ElementStride = static_cast<uint32_t>(elementLayout->getSize());
    }
}

/** Reads one `[vx_*]` annotation into its argument strings. Stage 3 never evaluates one, so a
 * malformed argument is caught here only when it is not a string at all. */
CookResult<void> SlangCompiler::Impl::CollectRawSizeAttributes(
    slang::VariableReflection* leaf_variable,
    std::string_view binding_name,
    std::vector<RawSizeAttribute>& out_attributes) const
{
    if (leaf_variable == nullptr)
    {
        return {};
    }

    for (const RawSizeAttributeKind kind : k_SizeAttributeKinds)
    {
        const std::string attributeName{ ToString(kind) };
        slang::Attribute* found =
            leaf_variable->findAttributeByName(GlobalSession.get(), attributeName.c_str());
        if (found == nullptr)
        {
            continue;
        }

        RawSizeAttribute attribute;
        attribute.Kind = kind;
        attribute.Arguments.reserve(ArgumentCountOf(kind));

        for (uint32_t i = 0u; i < ArgumentCountOf(kind); ++i)
        {
            CookResult<std::string> argument = ReadStringArgument(found, i, binding_name);
            if (!argument)
            {
                return std::unexpected(argument.error());
            }

            attribute.Arguments.push_back(std::move(argument.value()));
        }

        out_attributes.push_back(std::move(attribute));
    }

    return {};
}

CookResult<void> SlangCompiler::Impl::ExtractRawBindings(slang::ProgramLayout* program_layout,
                                                         std::vector<RawBinding>& out_bindings,
                                                         std::vector<RawSizeAttribute>& out_attributes)
{
    // The attributes of one resource travel beside it until the sort is done, because a sort that
    // moved the bindings alone would leave every attribute pointing at the wrong one.
    struct RawBindingDraft
    {
        RawBinding Binding;
        std::vector<RawSizeAttribute> Attributes;
    };

    std::vector<RawBindingDraft> drafts;

    slang::TypeLayoutReflection* globalLayout = program_layout->getGlobalParamsTypeLayout();
    if (globalLayout == nullptr)
    {
        return {};
    }

    const SlangInt bindingRangeCount = globalLayout->getBindingRangeCount();
    drafts.reserve(static_cast<size_t>(bindingRangeCount));

    for (SlangInt rangeIndex = 0; rangeIndex < bindingRangeCount; ++rangeIndex)
    {
        const slang::BindingType bindingType = globalLayout->getBindingRangeType(rangeIndex);
        // Skip input/output attributes, which slang considers to be part of the global params
        if (bindingType == slang::BindingType::Unknown || bindingType == slang::BindingType::VaryingInput ||
            bindingType == slang::BindingType::VaryingOutput)
        {
            continue;
        }

        const SlangInt descriptorSetIndex = globalLayout->getBindingRangeDescriptorSetIndex(rangeIndex);
        const SlangInt descriptorRangeIndex =
            globalLayout->getBindingRangeFirstDescriptorRangeIndex(rangeIndex);
        if (descriptorSetIndex < 0 || descriptorRangeIndex < 0)
        {
            continue;
        }

        const SlangInt spaceOffset = globalLayout->getDescriptorSetSpaceOffset(descriptorSetIndex);
        const SlangInt indexOffset = globalLayout->getDescriptorSetDescriptorRangeIndexOffset(
            descriptorSetIndex, descriptorRangeIndex);

        if (static_cast<size_t>(indexOffset) == SLANG_UNKNOWN_SIZE ||
            static_cast<size_t>(spaceOffset) == SLANG_UNKNOWN_SIZE)
        {
            std::println(stderr,
                         "[shader_cooker] binding range {} has an unresolved location: link-time "
                         "constants are affecting reflection output",
                         rangeIndex);
            continue;
        }

        slang::VariableReflection* leafVariable = globalLayout->getBindingRangeLeafVariable(rangeIndex);
        const char* leafName = leafVariable != nullptr ? leafVariable->getName() : nullptr;

        RawBindingDraft draft;
        draft.Binding.Name = leafName != nullptr ? leafName : std::string{};
        draft.Binding.Placement = BoundPlacement{ .Group = static_cast<uint32_t>(spaceOffset),
                                                  .Binding = static_cast<uint32_t>(indexOffset) };
        draft.Binding.Kind = FromSlangBindingType(bindingType);
        draft.Binding.ArrayCount =
            static_cast<uint32_t>(globalLayout->getBindingRangeBindingCount(rangeIndex));

        ApplyLeafTypeLayout(globalLayout, rangeIndex, bindingType, draft.Binding);

        if (CookResult<void> attributes =
                CollectRawSizeAttributes(leafVariable, draft.Binding.Name, draft.Attributes);
            !attributes)
        {
            return std::unexpected(attributes.error());
        }

        drafts.push_back(std::move(draft));
    }

    std::ranges::stable_sort(drafts,
                             [](const RawBindingDraft& lhs, const RawBindingDraft& rhs)
                             {
                                 return PlacementLess(lhs.Binding.Placement, rhs.Binding.Placement);
                             });

    out_bindings.reserve(drafts.size());
    for (RawBindingDraft& draft : drafts)
    {
        const uint32_t bindingIndex = static_cast<uint32_t>(out_bindings.size());
        for (RawSizeAttribute& attribute : draft.Attributes)
        {
            attribute.BindingIndex = bindingIndex;
            out_attributes.push_back(std::move(attribute));
        }

        out_bindings.push_back(std::move(draft.Binding));
    }

    return {};
}

void SlangCompiler::Impl::CollectUsedBindingIndices(slang::IComponentType* linked_program,
                                                    SlangInt entry_point_index,
                                                    const std::vector<RawBinding>& global_bindings,
                                                    std::vector<uint32_t>& out_used_indices)
{
    Slang::ComPtr<slang::IMetadata> metadata;
    Slang::ComPtr<slang::IBlob> diagnostics;
    if (SLANG_FAILED(linked_program->getEntryPointMetadata(
            entry_point_index, k_WgslTargetIndex, metadata.writeRef(), diagnostics.writeRef())) ||
        metadata == nullptr)
    {
        ReportDiagnostics(*Sink, "getEntryPointMetadata", diagnostics.get());
        return;
    }

    out_used_indices.reserve(global_bindings.size());

    for (size_t i = 0u; i < global_bindings.size(); ++i)
    {
        const BoundPlacement* placement = GetBoundPlacement(global_bindings[i].Placement);
        if (placement == nullptr)
        {
            continue;
        }

        bool isUsed = false;
        metadata->isParameterLocationUsed(
            SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT, placement->Group, placement->Binding, isUsed);
        if (isUsed)
        {
            out_used_indices.push_back(static_cast<uint32_t>(i));
        }
    }
}

CookResult<RawEntryPoint> SlangCompiler::Impl::ExtractRawEntryPoint(
    slang::IComponentType* linked_program,
    slang::ProgramLayout* program_layout,
    SlangInt entry_point_index,
    const std::vector<RawBinding>& global_bindings)
{
    RawEntryPoint rawEntryPoint;
    rawEntryPoint.Name = EntryPointNames[static_cast<size_t>(entry_point_index)];

    slang::EntryPointReflection* entryPointLayout =
        program_layout->getEntryPointByIndex(static_cast<SlangUInt>(entry_point_index));
    if (entryPointLayout == nullptr)
    {
        return rawEntryPoint;
    }

    rawEntryPoint.Stage = FromSlangStage(entryPointLayout->getStage());

    if (rawEntryPoint.Stage == ShaderStageKind::Compute)
    {
        std::array<SlangUInt, k_WorkgroupAxisCount> workgroupSizes{ 1u, 1u, 1u };
        entryPointLayout->getComputeThreadGroupSize(k_WorkgroupAxisCount, workgroupSizes.data());
        rawEntryPoint.Workgroup.X = static_cast<uint32_t>(workgroupSizes[0]);
        rawEntryPoint.Workgroup.Y = static_cast<uint32_t>(workgroupSizes[1]);
        rawEntryPoint.Workgroup.Z = static_cast<uint32_t>(workgroupSizes[2]);
    }

    ExtractRasterState(entryPointLayout, rawEntryPoint.Stage, rawEntryPoint.Raster);

    CollectUsedBindingIndices(
        linked_program, entry_point_index, global_bindings, rawEntryPoint.UsedBindingIndices);
    return rawEntryPoint;
}

void SlangCompiler::Impl::ExtractRasterState(slang::EntryPointReflection* entry_point_layout,
                                             ShaderStageKind stage,
                                             ReflectedRasterState& raster)
{
    if (stage == ShaderStageKind::Vertex)
    {
        CollectVertexInputs(entry_point_layout->getVarLayout(), 0u, raster);
        return;
    }

    if (stage != ShaderStageKind::Fragment)
    {
        return;
    }

    CollectColorTargets(entry_point_layout->getResultVarLayout(), 0u, raster);
    CollectDepthWrites(entry_point_layout->getVarLayout(), raster);
}

SlangCompiler::SlangCompiler() noexcept
    : impl{ nullptr }
{
}

SlangCompiler::~SlangCompiler() = default;
SlangCompiler::SlangCompiler(SlangCompiler&&) noexcept = default;
SlangCompiler& SlangCompiler::operator=(SlangCompiler&&) noexcept = default;

CookResult<void> SlangCompiler::Initialize(const SlangCompilerCreateInfo& create_info, DiagnosticSink& sink)
{
    impl = std::make_unique<Impl>();
    impl->Sink = &sink;

    const CookResult<void> sessionResult = impl->CreateSession(create_info);
    if (!sessionResult)
    {
        return sessionResult;
    }

    const CookResult<void> moduleResult = impl->LoadRootModule();
    if (!moduleResult)
    {
        return moduleResult;
    }

    return impl->CollectEntryPoints();
}

CookResult<RawModule> SlangCompiler::PrepareRawModule(const PermutationSpace& space)
{
    if (impl == nullptr)
    {
        return std::unexpected(CookError::CompilerNotInitialized);
    }

    CookResult<std::vector<ExternConstantDefault>> defaults =
        CollectUndrivenExternDefaults(space, impl->ModuleSourceTexts);
    if (!defaults)
    {
        return std::unexpected(defaults.error());
    }

    RawModule module;
    module.Name = impl->ModuleName;
    module.EntryPointNames = impl->EntryPointNames;
    module.ExternDefaults = std::move(defaults.value());
    return module;
}

CookResult<RawVariant> SlangCompiler::CompileVariantRaw(const VariantDescriptor& descriptor)
{
    if (impl == nullptr)
    {
        return std::unexpected(CookError::CompilerNotInitialized);
    }

    CookResult<Slang::ComPtr<slang::IComponentType>> linkResult = impl->LinkVariant(descriptor.Active);
    if (!linkResult)
    {
        return std::unexpected(linkResult.error());
    }

    slang::IComponentType* linkedProgram = linkResult.value().get();
    slang::ProgramLayout* programLayout = linkedProgram->getLayout();
    if (programLayout == nullptr)
    {
        return std::unexpected(CookError::ReflectionUnavailable);
    }

    const std::vector<std::string> generatedCode = impl->GenerateEntryPointCode(linkedProgram);

    RawVariant variant;
    variant.VariantSuffix = MakeAssignmentSuffix(descriptor.Canonical);
    variant.VariantDescription = DescribeAssignment(descriptor.Canonical);
    variant.VariantIndex = descriptor.Index;

    // Slang reports the bindings for the whole program, so this set is the same for every entry point
    // of this variant. Extract it once, and let each entry point say which of them it reads.
    if (CookResult<void> bindings =
            impl->ExtractRawBindings(programLayout, variant.GlobalBindings, variant.SizeAttributes);
        !bindings)
    {
        return std::unexpected(bindings.error());
    }

    variant.EntryPoints.reserve(impl->EntryPointNames.size());

    for (size_t i = 0; i < impl->EntryPointNames.size(); ++i)
    {
        if (generatedCode[i].empty())
        {
            return std::unexpected(CookError::CodeGenerationFailed);
        }

        CookResult<RawEntryPoint> rawEntryPoint = impl->ExtractRawEntryPoint(
            linkedProgram, programLayout, static_cast<SlangInt>(i), variant.GlobalBindings);
        if (!rawEntryPoint)
        {
            return std::unexpected(rawEntryPoint.error());
        }

        rawEntryPoint.value().VariantSuffix = variant.VariantSuffix;
        rawEntryPoint.value().TargetText = generatedCode[i];
        variant.EntryPoints.push_back(std::move(rawEntryPoint.value()));
    }

    return variant;
}

std::string_view SlangCompiler::GetModuleName() const noexcept
{
    if (impl == nullptr)
    {
        return {};
    }

    return impl->ModuleName;
}

std::span<const std::string> SlangCompiler::GetEntryPointNames() const noexcept
{
    if (impl == nullptr)
    {
        return {};
    }

    return impl->EntryPointNames;
}

std::span<const std::string> SlangCompiler::GetModuleSourceTexts() const noexcept
{
    if (impl == nullptr)
    {
        return {};
    }

    return impl->ModuleSourceTexts;
}

} // namespace lodestone
